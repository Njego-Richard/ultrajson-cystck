/*
Developed by ESN, an Electronic Arts Inc. studio.
Copyright (c) 2014, Electronic Arts Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ESN, Electronic Arts Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS INC. BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Portions of code from MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007  Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

Numeric decoder derived from from TCL library
http://www.opensource.apple.com/source/tcl/tcl-14/tcl/license.terms
* Copyright (c) 1988-1993 The Regents of the University of California.
* Copyright (c) 1994 Sun Microsystems, Inc.
*/

#include "../../include/Cystck.h"
#include <stdio.h>
#include "../lib/ultrajson.h"

#define EPOCH_ORD 719163

typedef void *(*PFN_PyTypeToJSON)(JSOBJ obj, JSONTypeContext *ti, void *outValue, size_t *_outLen);

int object_is_decimal_type(Py_State *S, Cystck_Object obj);

typedef struct __TypeContext
{
  JSPFN_ITEREND iterEnd;
  JSPFN_ITERNEXT iterNext;
  JSPFN_ITERGETNAME iterGetName;
  JSPFN_ITERGETVALUE iterGetValue;
  PFN_PyTypeToJSON PyTypeToJSON;
  Cystck_Object newObj;
  Cystck_Object dictObj;
  Cystck_ssize_t index;
  Cystck_ssize_t size;
  Cystck_Object itemValue;
  Cystck_Object itemName;
  Cystck_Object attrList;
  Cystck_Object iterator;

  union
  {
    Cystck_Object rawJSONValue;
    JSINT64 longValue;
    JSUINT64 unsignedLongValue;
  };
} TypeContext;

#define GET_TC(__ptrtc) ((TypeContext *)((__ptrtc)->prv))
#define GET_PyState(__ptrtc) ((Py_State *)((__ptrtc)->encoder_prv))
// If newObj is set, we should use it rather than JSOBJ
#define GET_OBJ(__jsobj, __ptrtc) (GET_TC(__ptrtc)->newObj ? GET_TC(__ptrtc)->newObj : __jsobj)

// Avoid infinite loop caused by the default function
#define DEFAULT_FN_MAX_DEPTH 3

struct PyDictIterState
{
  PyObject *keys;
  size_t i;
  size_t sz;
};

//#define PRINTMARK() fprintf(stderr, "%s: MARK(%d)\n", __FILE__, __LINE__)
#define PRINTMARK()

static void *PyLongToINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSINT64 *) outValue) = GET_TC(tc)->longValue;
  return NULL;
}

static void *PyLongToUINT64(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  *((JSUINT64 *) outValue) = GET_TC(tc)->unsignedLongValue;
  return NULL;
}

static void *PyLongToINTSTR(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object object = Cystck_FromVoid(_obj);
  Cystck_Object obj =  CystckNumber_Tobase(S, object,10);
  if (Cystck_IsNULL(obj))
  {
    return NULL;
  }
  *_outLen = PyUnicode_GET_LENGTH(obj);
  return PyUnicode_1BYTE_DATA(obj);
}

static void *PyFloatToDOUBLE(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  Cystck_Object obj = Cystck_FromVoid(_obj);
  *((double *) outValue) = CystckFloat_AsDouble (GET_PyState(tc),obj);
  return NULL;
}

static void *PyStringToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object obj = Cystck_FromVoid(_obj);
  *_outLen = CystckBytes_GET_SIZE(S,obj);
  return CystckBytes_AS_STRING(S,obj);
}

static char *PyUnicodeToUTF8Raw(JSOBJ _obj, size_t *_outLen, Cystck_Object *pBytesObj)
{
  /*
  Converts the PyUnicode object to char* whose size is stored in _outLen.
  This conversion may require the creation of an intermediate PyBytes object.
  In that case, the returned char* is in fact the internal buffer of that PyBytes object,
  and when the char* buffer is no longer needed, the bytesObj must be DECREF'd.
  */
  Cystck_Object obj = Cystck_FromVoid(_obj);
  Py_State *S = Get_State();
#ifndef Py_LIMITED_API
  if (CystckUnicode_IS_COMPACT_ASCII(obj))
  {
    Cystck_ssize_t len;
    char *data = CystckUnicode_AsUTF8AndSize(S,obj, &len);
    *_outLen = len;
    return data;
  }
#endif

  
  Cystck_Object bytesObj = *pBytesObj = CystckUnicode_AsEncodedString(S, obj, NULL, "surrogatepass");
  if ( Cystck_IsNULL(bytesObj))
  {
    return NULL;
  }

  *_outLen = CystckBytes_Size(S,bytesObj);
  return CystckBytes_AsString(S,bytesObj);
}

static void *PyUnicodeToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  return PyUnicodeToUTF8Raw(_obj, _outLen, &(GET_TC(tc)->newObj));
}

static void *PyRawJSONToUTF8(JSOBJ _obj, JSONTypeContext *tc, void *outValue, size_t *_outLen)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object obj = GET_TC(tc)->rawJSONValue;
  if (CystckUnicode_Check(S,obj))
  {
    return PyUnicodeToUTF8( _obj, tc, outValue, _outLen);
  }
  else
  {
    return PyStringToUTF8(_obj, tc, outValue, _outLen);
  }
}

static int Tuple_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  Cystck_Object item;
  Py_State *S = GET_PyState(tc);
  Cystck_Object _obj = Cystck_FromVoid(obj);

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    return 0;
  }

  item = CystckTuple_GetItem(S,_obj, GET_TC(tc)->index);

  GET_TC(tc)->itemValue = item;
  GET_TC(tc)->index ++;
  return 1;
}

static void Tuple_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ Tuple_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return Cystck_AsVoidP(GET_TC(tc)->itemValue);
}

static char *Tuple_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

static int List_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object _obj = Cystck_FromVoid(obj); 
  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  GET_TC(tc)->itemValue = CystckList_GetItem(S, _obj, GET_TC(tc)->index);
  GET_TC(tc)->index ++;
  return 1;
}

static void List_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
}

static JSOBJ List_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return Cystck_AsVoidP (GET_TC(tc)->itemValue);
}

static char *List_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  return NULL;
}

//=============================================================================
// Dict iteration functions
// itemName might converted to string (Python_Str). Do refCounting
// itemValue is borrowed from object (which is dict). No refCounting
//=============================================================================

static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object itemNameTmp;
  Cystck_pushobject(S, GET_TC(tc)->itemName);

  if (!Cystck_IsNULL(GET_TC(tc)->itemName))
  {
    Cystck_pop(S,GET_TC(tc)->itemName);
    GET_TC(tc)->itemName = 0;
  }

  if ( Cystck_IsNULL(GET_TC(tc)->itemName = CystckIter_Next(S, GET_TC(tc)->iterator)))
  {
    PRINTMARK();
    return 0;
  }

  if (Cystck_IsNULL(GET_TC(tc)->itemValue = CystckDict_GetItem(S, GET_TC(tc)->dictObj, GET_TC(tc)->itemName)))
  {
    PRINTMARK();
    return 0;
  }

  if (CystckUnicode_Check(S, GET_TC(tc)->itemName))
  {
    itemNameTmp = GET_TC(tc)->itemName;
    Cystck_pushobject(S, itemNameTmp);
    GET_TC(tc)->itemName = CystckUnicode_AsEncodedString (S,GET_TC(tc)->itemName, NULL, "surrogatepass");
    Cystck_pop(S,itemNameTmp);
  }
  else
  if (!CystckBytes_Check(S,GET_TC(tc)->itemName))
  {
    if (UNLIKELY(GET_TC(tc)->itemName == S->Cystck_None))
    {
      Cystck_pushobject(S, S->Cystck_None);
      itemNameTmp = CystckUnicode_FromString(S,"null");
      GET_TC(tc)->itemName = CystckUnicode_AsUTF8String(S, itemNameTmp);
      Cystck_pop(S,S->Cystck_None);
      return 1;
    }

    itemNameTmp = GET_TC(tc)->itemName;
    Cystck_pushobject(S, itemNameTmp);
    GET_TC(tc)->itemName = Cystck_Str(S,GET_TC(tc)->itemName);
    Cystck_pop(S, itemNameTmp);
    if (Cystck_Err_Occurred(S))
    {
      PRINTMARK();
      return -1;
    }
    itemNameTmp = GET_TC(tc)->itemName;
    Cystck_pushobject(S, itemNameTmp);
    GET_TC(tc)->itemName = CystckUnicode_AsEncodedString (S, GET_TC(tc)->itemName, NULL, "surrogatepass");
    Cystck_pop(S, itemNameTmp);
  }
  PRINTMARK();
  return 1;
}

static void Dict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  Py_State *S = GET_PyState(tc);
  Cystck_pushobject(S,GET_TC(tc)->itemName);
  if (!Cystck_IsNULL(GET_TC(tc)->itemName))
  {
    Cystck_pop(S,GET_TC(tc)->itemName);
    GET_TC(tc)->itemName = 0;
  }
  Cystck_pushobject(S,GET_TC(tc)->iterator);
  if (!Cystck_IsNULL(GET_TC(tc)->iterator))
  {
    Cystck_pop(S,GET_TC(tc)->iterator);
    GET_TC(tc)->itemName = 0;
  }
  PRINTMARK();
}

static JSOBJ Dict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return Cystck_AsVoidP(GET_TC(tc)->itemValue);
}

static char *Dict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  Py_State *S = GET_PyState(tc);
  *outLen = CystckBytes_GET_SIZE(S,GET_TC(tc)->itemName);
  return CystckBytes_AS_STRING(S,GET_TC(tc)->itemName);
}

static int SortedDict_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  Cystck_Object items = 0, item = 0, key = 0, value = 0;
  Py_State *S = GET_PyState(tc);
  Cystck_ssize_t i, nitems;
  Cystck_Object keyTmp;

  // Upon first call, obtain a list of the keys and sort them. This follows the same logic as the
  // standard library's _json.c sort_keys handler.
  if ( Cystck_IsNULL(GET_TC(tc)->newObj))
  {
    // Obtain the list of keys from the dictionary.
    items = CystckMapping_Keys(S, GET_TC(tc)->dictObj);
    if (Cystck_IsNULL(items))
    {
      goto error;
    }
    else if (!CystckList_Check(S, items))
    {
      CystckErr_SetString (S, S->Cystck_ValueError, "keys must return list");
      goto error;
    }

    // Sort the list.
    if (CystckList_Sort(S,items) < 0)
    {
      CystckErr_SetString (S, S->Cystck_ValueError, "unorderable keys");
      goto error;
    }

    // Obtain the value for each key, and pack a list of (key, value) 2-tuples.
    nitems = CystckList_GET_SIZE(items);
    for (i = 0; i < nitems; i++)
    {
      key = CystckList_GetItem(S, items,i);
      Cystck_pushobject(S,key);
      value = CystckDict_GetItem (S,GET_TC(tc)->dictObj, key);
      Cystck_pushobject(S,value);

      // Subject the key to the same type restrictions and conversions as in Dict_iterGetValue.
      if (CystckUnicode_Check(S,key))
      {
        key = CystckUnicode_AsEncodedString(S, key, NULL, "surrogatepass");
        Cystck_pushobject(S, key);
      }
      else if (!CystckBytes_Check(S,key))
      {
        key = Cystck_Str(S, key);
        Cystck_pushobject(S,key);
        if (Cystck_Err_Occurred(S))
        {
          goto error;
        }
        keyTmp = key;
        Cystck_pushobject(S,keyTmp);
        key = CystckUnicode_AsUTF8String(S,key);
        Cystck_pop(S,keyTmp);
      }
      else
      {
        Cystck_pushobject(S,key);
      }

      item = Cystck_Tuple_Pack(S,2, key, value);
      Cystck_pushobject(S,item);
      if (Cystck_IsNULL(item) )
      {
        goto error;
      }
      if (CystckList_SetItem(S, items, i, item))
      {
        goto error;
      }
      Cystck_pop(S,key);
    }

    // Store the sorted list of tuples in the newObj slot.
    GET_TC(tc)->newObj = items;
    Cystck_pushobject(S, GET_TC(tc)->newObj );
    GET_TC(tc)->size = nitems;
  }

  if (GET_TC(tc)->index >= GET_TC(tc)->size)
  {
    PRINTMARK();
    return 0;
  }

  item = CystckList_GetItem(S, GET_TC(tc)->newObj, GET_TC(tc)->index);
  GET_TC(tc)->itemName = CystckTuple_GET_ITEM(item, 0);
  GET_TC(tc)->itemValue = CystckTuple_GET_ITEM(item, 1);
  GET_TC(tc)->index++;
  return 1;

error:
  Cystck_pop(S, item);
  Cystck_pop(S, key);
  Cystck_pop(S, value);
  Cystck_pop(S, items);
  return -1;
}

static void SortedDict_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  Py_State *S = GET_PyState(tc);
  GET_TC(tc)->itemName = 0;
  GET_TC(tc)->itemValue = 0;
  //Cystck_pop(S, GET_TC(tc)->dictObj);
  PRINTMARK();
}

static JSOBJ SortedDict_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  return Cystck_AsVoidP(GET_TC(tc)->itemValue);
}

static char *SortedDict_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  Py_State *S = GET_PyState(tc);
  *outLen = CystckBytes_GET_SIZE(S,GET_TC(tc)->itemName); 
  return CystckBytes_AS_STRING(S,GET_TC(tc)->itemName);
}

static void SetupDictIter(Py_State *S, Cystck_Object dictObj, TypeContext *pc, JSONObjectEncoder *enc)
{
  pc->dictObj = dictObj;
  //Cystck_pop(S, pc->dictObj);
  if (enc->sortKeys)
  {
    pc->iterEnd = SortedDict_iterEnd;
    pc->iterNext = SortedDict_iterNext;
    pc->iterGetValue = SortedDict_iterGetValue;
    pc->iterGetName = SortedDict_iterGetName;
    pc->index = 0;
  }
  else
  {
    pc->iterEnd = Dict_iterEnd;
    pc->iterNext = Dict_iterNext;
    pc->iterGetValue = Dict_iterGetValue;
    pc->iterGetName = Dict_iterGetName;
    pc->iterator = Cystck_GetIter(S, dictObj);
  }
}

static void Object_beginTypeContext (JSOBJ _obj, JSONTypeContext *tc, JSONObjectEncoder *enc)
{
  Py_State *S = GET_PyState(tc);
  Cystck_Object obj, objRepr, defaultFn, newObj;
  int level = 0;
  TypeContext *pc;
  PRINTMARK();
  if (!_obj)
  {
    tc->type = JT_INVALID;
    return;
  }

  obj = Cystck_FromVoid( _obj);
  defaultFn = Cystck_FromVoid( enc->prv);

  tc->prv = malloc(sizeof(TypeContext));
  pc = (TypeContext *) tc->prv;
  if (!pc)
  {
    tc->type = JT_INVALID;
    CystckErr_NoMemory(S);
    return;
  }
  pc->newObj = 0;
  pc->dictObj = 0;
  pc->itemValue = 0;
  pc->itemName = 0;
  pc->iterator = 0;
  pc->attrList = 0;
  pc->index = 0;
  pc->size = 0;
  pc->longValue = 0;
  pc->rawJSONValue = 0;

BEGIN:
  if (CystckIter_Check(S, obj))
  {
    PRINTMARK();
    goto ISITERABLE;
  }

  if (CystckBool_check(S,obj))
  {
    PRINTMARK();
    tc->type = Cystck_IsTrue(S,obj) ? JT_TRUE : JT_FALSE;
    return;
  }
  else
  if (CystckLong_check(S,obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyLongToINT64;
    tc->type = JT_LONG;
    GET_TC(tc)->longValue = CystckLong_AsLongLong(S, obj);

    if (!Cystck_Err_Occurred (S) )
    {
        return;
    }

    if (Cystck_Err_Occurred (S) && CystckErr_EXceptionMatches(S,PyExc_OverflowError))
    {
      Cystck_Err_Clear(S);
      pc->PyTypeToJSON = PyLongToUINT64;
      tc->type = JT_ULONG;
      GET_TC(tc)->unsignedLongValue = CystckLong_AsUnsignedLongLong(S, obj);

    }

    if (Cystck_Err_Occurred (S) && CystckErr_EXceptionMatches(S, PyExc_OverflowError))
    {
      Cystck_Err_Clear(S);
      pc->PyTypeToJSON = PyLongToINTSTR;
      tc->type = JT_RAW;
      // Overwritten by PyLong_* due to the union, which would lead to a DECREF in endTypeContext.
      GET_TC(tc)->rawJSONValue = 0;
      return;
    }

    if (Cystck_Err_Occurred (S))
    {
      PRINTMARK();
      goto INVALID;
    }

    return;
  }
  else
  if (UNLIKELY(CystckBytes_Check(S,obj)))
  {
    PRINTMARK();
    if (enc->rejectBytes)
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "reject_bytes is on ");
      goto INVALID;
    }
    else
    {
      pc->PyTypeToJSON = PyStringToUTF8; tc->type = JT_UTF8;
      return;
    }
  }
  else
  if (CystckUnicode_Check(S,obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyUnicodeToUTF8; tc->type = JT_UTF8;
    return;
  }
  else
  if (obj == S->Cystck_None)
  {
    PRINTMARK();
    tc->type = JT_NULL;
    return;
  }
  else
  if (CystckFloat_check(S, obj) || object_is_decimal_type(S, obj))
  {
    PRINTMARK();
    pc->PyTypeToJSON = PyFloatToDOUBLE; tc->type = JT_DOUBLE;
    return;
  }

ISITERABLE:
  if (CystckDict_Check(S, obj))
  {
    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(S, obj, pc, enc);
    Cystck_pushobject(S,obj);
    return;
  }
  else
  if (CystckList_Check(S, obj))
  {
    PRINTMARK();
    tc->type = JT_ARRAY;
    pc->iterEnd = List_iterEnd;
    pc->iterNext = List_iterNext;
    pc->iterGetValue = List_iterGetValue;
    pc->iterGetName = List_iterGetName;
    GET_TC(tc)->index =  0;
    GET_TC(tc)->size = CystckList_GET_SIZE(obj);
    return;
  }
  else
  if (CystckTuple_Check(S, obj))
  {
    PRINTMARK();
    tc->type = JT_ARRAY;
    pc->iterEnd = Tuple_iterEnd;
    pc->iterNext = Tuple_iterNext;
    pc->iterGetValue = Tuple_iterGetValue;
    pc->iterGetName = Tuple_iterGetName;
    GET_TC(tc)->index = 0;
    GET_TC(tc)->size = CystckTuple_GET_SIZE(obj);
    GET_TC(tc)->itemValue = 0;

    return;
  }

  if (UNLIKELY(Cystck_HasAttrString(S, obj, "toDict")))
  {
    Cystck_Object toDictFunc = Cystck_GetAttr_String(S,obj,"toDict");
    Cystck_pushobject(S,toDictFunc);
    Cystck_Object tuple = CystckTuple_New(S,0);
    Cystck_pushobject(S,tuple);
    Cystck_Object toDictResult = Cystck_Call(S,toDictFunc, tuple, 0);
    Cystck_pushobject(S,toDictResult);
    Cystck_pop(S, tuple);
    Cystck_pop(S, toDictFunc);

    if (Cystck_IsNULL(toDictResult))
    {
      goto INVALID;
    }

    if (!CystckDict_Check(S,toDictResult))
    {
      Cystck_pop(S, toDictResult);
      tc->type = JT_NULL;
      return;
    }

    PRINTMARK();
    tc->type = JT_OBJECT;
    SetupDictIter(S, toDictResult, pc, enc);
    return;
  }
  else
  if (UNLIKELY(Cystck_HasAttrString(S, obj, "__json__")))
  {
    Cystck_Object toJSONFunc = Cystck_GetAttr_String(S,obj, "__json__");
    Cystck_pushobject(S, toJSONFunc);
    Cystck_Object tuple = CystckTuple_New(S,0);
    Cystck_pushobject(S, tuple);
    Cystck_Object toJSONResult = Cystck_Call(S,toJSONFunc, tuple, 0);
    Cystck_pushobject(S, toJSONResult);
    Cystck_pop(S, tuple);
    Cystck_pop(S, toJSONFunc);

    if (Cystck_IsNULL(toJSONResult))
    {
      goto INVALID;
    }

    if (Cystck_Err_Occurred(S))
    {
      Cystck_pop(S, toJSONResult);
      goto INVALID;
    }

    if (!CystckBytes_Check(S,toJSONResult) && !CystckUnicode_Check(S,toJSONResult))
    {
      Cystck_pop(S, toJSONResult);
      CystckErr_SetString (S, S->Cystck_TypeError, "expected string");
      goto INVALID;
    }

    PRINTMARK();
    pc->PyTypeToJSON = PyRawJSONToUTF8;
    tc->type = JT_RAW;
    GET_TC(tc)->rawJSONValue = toJSONResult;
    Cystck_pushobject(S, GET_TC(tc)->rawJSONValue);
    return;
  }

DEFAULT:
  if (!Cystck_IsNULL(defaultFn))
  {
    // Break infinite loop
    if (level >= DEFAULT_FN_MAX_DEPTH)
    {
      PRINTMARK();
      CystckErr_SetString (S, S->Cystck_TypeError, "maximum recursion depth exceeded");
      goto INVALID;
    }

    newObj = Py2Cystck(PyObject_CallFunctionObjArgs( Cystck2py(defaultFn), Cystck2py(obj), NULL));
    Cystck_pushobject(S, pc->newObj);
    if (!Cystck_IsNULL(newObj))
    {
      PRINTMARK();
      Cystck_pop(S, pc->newObj);
      obj = pc->newObj = newObj;
      level += 1;
      goto BEGIN;
    }
    else
    {
      goto INVALID;
    }
  }

  PRINTMARK();
  Cystck_Err_Clear(S);

  objRepr = Cystck_Repr(S, obj);
  Cystck_pushobject(S,objRepr);
  if ( Cystck_IsNULL(objRepr))
  {
    goto INVALID;
  }
  Cystck_Object str = CystckUnicode_AsEncodedString(S,objRepr, NULL, "strict");
  Cystck_pushobject(S,str);
  if (!Cystck_IsNULL(str))
  {
    CystckErr_SetString (S, S->Cystck_TypeError, "object is not JSON serializable");
  }
  Cystck_pop(S,str);
  Cystck_pop(S,objRepr);

INVALID:
  PRINTMARK();
  tc->type = JT_INVALID;
  free(tc->prv);
  tc->prv = NULL;
  return;
}

static void Object_endTypeContext(JSOBJ obj, JSONTypeContext *tc)
{
  Py_State *S = GET_PyState(tc);
  //Cystck_pop(S, GET_TC(tc)->newObj);

  if (tc->type == JT_RAW)
  {
    //Cystck_pop(S, GET_TC(tc)->rawJSONValue);
  }
  free(tc->prv);
  tc->prv = NULL;
}

static const char *Object_getStringValue(JSOBJ obj, JSONTypeContext *tc, size_t *_outLen)
{
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  return GET_TC(tc)->PyTypeToJSON (obj, tc, NULL, _outLen);
}

static JSINT64 Object_getLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSINT64 ret;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static JSUINT64 Object_getUnsignedLongValue(JSOBJ obj, JSONTypeContext *tc)
{
  JSUINT64 ret;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static double Object_getDoubleValue(JSOBJ obj, JSONTypeContext *tc)
{
  double ret;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  GET_TC(tc)->PyTypeToJSON (obj, tc, &ret, NULL);
  return ret;
}

static void Object_releaseObject(JSOBJ _obj)
{
  Cystck_Object obj = Cystck_FromVoid(_obj);
}

static int Object_iterNext(JSOBJ obj, JSONTypeContext *tc)
{
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);  
  return GET_TC(tc)->iterNext(obj, tc);
}

static void Object_iterEnd(JSOBJ obj, JSONTypeContext *tc)
{
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  GET_TC(tc)->iterEnd(obj, tc);
}

static JSOBJ Object_iterGetValue(JSOBJ obj, JSONTypeContext *tc)
{
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  return GET_TC(tc)->iterGetValue(obj, tc);
}

static char *Object_iterGetName(JSOBJ obj, JSONTypeContext *tc, size_t *outLen)
{
  Cystck_Object _obj = Cystck_FromVoid(obj);
  _obj = GET_OBJ(_obj, tc);
  obj = Cystck_AsVoidP(_obj);
  return GET_TC(tc)->iterGetName(obj, tc, outLen);
}

Cystck_Object objToJSON(Py_State *S, Cystck_Object args, Cystck_Object kwargs)
{
  static char *kwlist[] = { "obj", "ensure_ascii", "encode_html_chars", "escape_forward_slashes", "sort_keys", "indent", "allow_nan", "reject_bytes", "default", "separators", NULL };

  char buffer[65536];
  char *ret;
  const char *csNan = NULL, *csInf = NULL;
  Cystck_Object newobj;
  Cystck_Object oinput = 0;
  Cystck_Object oensureAscii = 0;
  Cystck_Object oencodeHTMLChars = 0;
  Cystck_Object oescapeForwardSlashes = 0;
  Cystck_Object osortKeys = 0;
  Cystck_Object odefaultFn = 0;
  Cystck_Object oseparators = 0;
  Cystck_Object oseparatorsItem = 0;
  Cystck_Object separatorsItemBytes = 0;
  Cystck_Object oseparatorsKey = 0;
  Cystck_Object separatorsKeyBytes = 0;
  int allowNan = -1;
  int orejectBytes = -1;
  size_t retLen;

  JSONObjectEncoder encoder =
  {
    Object_beginTypeContext,
    Object_endTypeContext,
    Object_getStringValue,
    Object_getLongValue,
    Object_getUnsignedLongValue,
    Object_getDoubleValue,
    Object_iterNext,
    Object_iterEnd,
    Object_iterGetValue,
    Object_iterGetName,
    Object_releaseObject,
    malloc,
    realloc,
    free,
    -1, //recursionMax
    1, //forceAscii
    0, //encodeHTMLChars
    1, //escapeForwardSlashes
    0, //sortKeys
    0, //indent
    1, //allowNan
    1, //rejectBytes
    0, //itemSeparatorLength
    NULL, //itemSeparatorChars
    0, //keySeparatorLength
    NULL, //keySeparatorChars
    NULL, //prv
  };

  encoder.prv =S;
  PRINTMARK();

  if (!_PyArg_parseTupleAndKeywords(S, args, kwargs, "O|OOOOiiiOO", kwlist, &oinput, &oensureAscii, &oencodeHTMLChars, &oescapeForwardSlashes, &osortKeys, &encoder.indent, &allowNan, &orejectBytes, &odefaultFn, &oseparators))
  {
    return -1;
  }
  void *_oinput = Cystck_AsVoidP(oinput);

  if (!Cystck_IsNULL(oensureAscii) && !Cystck_IsTrue(S, oensureAscii))
  {
    encoder.forceASCII = 0;
  }

  if (!Cystck_IsNULL(oencodeHTMLChars) && Cystck_IsTrue(S, oencodeHTMLChars))
  {
    encoder.encodeHTMLChars = 1;
  }

  if (!Cystck_IsNULL(oescapeForwardSlashes) && !Cystck_IsTrue(S, oescapeForwardSlashes))
  {
    encoder.escapeForwardSlashes = 0;
  }

  if (!Cystck_IsNULL(osortKeys) && Cystck_IsTrue(S, osortKeys))
  {
    encoder.sortKeys = 1;
  }

  if (allowNan != -1)
  {
    encoder.allowNan = allowNan;
  }

  if (!Cystck_IsNULL(odefaultFn) && odefaultFn != S->Cystck_None)
  {
    // Here use prv to store default function
    encoder.prv = Cystck_AsVoidP(odefaultFn);
  }

  if (encoder.allowNan)
  {
    csInf = "Infinity";
    csNan = "NaN";
  }

  if (orejectBytes != -1)
  {
    encoder.rejectBytes = orejectBytes;
  }

  if (!Cystck_IsNULL(oseparators) && oseparators != S->Cystck_None)
  {
    if (!CystckTuple_Check(S, oseparators))
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "expected tuple or None as separator");
      return -1;
    }
    if (CystckTuple_Size(S, oseparators) != 2)
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "expected tuple of size 2 as separator");
      return -1;
    }
    oseparatorsItem = CystckTuple_GetItem(S,oseparators, 0);
    if (Cystck_Err_Occurred(S))
    {
      return -1;
    }
    if (!CystckUnicode_Check(S,oseparatorsItem))
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "expected str as item separator");
      return -1;
    }
    oseparatorsKey = CystckTuple_GetItem(S, oseparators, 1);
    if (Cystck_Err_Occurred(S))
    {
      return -1;
    }
    if (!CystckUnicode_Check(S,oseparatorsKey))
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "expected str as key separator");
      return -1;
    }
    encoder.itemSeparatorChars = PyUnicodeToUTF8Raw( Cystck_AsVoidP(oseparatorsItem), &encoder.itemSeparatorLength, &separatorsItemBytes);
    Cystck_pushobject(S, separatorsItemBytes);
    if (encoder.itemSeparatorChars == NULL)
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "item separator malformed");
      goto ERROR;
    }
    encoder.keySeparatorChars = PyUnicodeToUTF8Raw(Cystck_AsVoidP(oseparatorsKey), &encoder.keySeparatorLength, &separatorsKeyBytes);
    Cystck_pushobject(S, separatorsKeyBytes);
    if (encoder.keySeparatorChars == NULL)
    {
      CystckErr_SetString (S, S->Cystck_TypeError, "key separator malformed");
      goto ERROR;
    }
  }
  else
  {
    // Default to most compact representation
    encoder.itemSeparatorChars = ",";
    encoder.itemSeparatorLength = 1;
    if (encoder.indent)
    {
      // Extra space when indentation is in use
      encoder.keySeparatorChars = ": ";
      encoder.keySeparatorLength = 2;
    }
    else
    {
      encoder.keySeparatorChars = ":";
      encoder.keySeparatorLength = 1;
    }
  }

  encoder.d2s = NULL;
  dconv_d2s_init(&encoder.d2s, DCONV_D2S_EMIT_TRAILING_DECIMAL_POINT | DCONV_D2S_EMIT_TRAILING_ZERO_AFTER_POINT | DCONV_D2S_EMIT_POSITIVE_EXPONENT_SIGN,
                 csInf, csNan, 'e', DCONV_DECIMAL_IN_SHORTEST_LOW, DCONV_DECIMAL_IN_SHORTEST_HIGH, 0, 0);

  PRINTMARK();
  ret = JSON_EncodeObject (_oinput, &encoder, buffer, sizeof (buffer), &retLen);
  PRINTMARK();

  dconv_d2s_free(&encoder.d2s);
  Cystck_pop(S, separatorsItemBytes);
  Cystck_pop(S, separatorsKeyBytes);

  if (encoder.errorMsg && !Cystck_Err_Occurred(S))
  {
    // If there is an error message and we don't already have a Python exception, set one.
    CystckErr_SetString (S, S->Cystck_OverflowError, encoder.errorMsg);
  }

  if (Cystck_Err_Occurred(S))
  {
    if (ret != buffer)
    {
      encoder.free (ret);
    }

    return -1;
  }

  newobj = CystckUnicode_DecodeUTF8(ret, retLen, "surrogatepass");

  if (ret != buffer)
  {
    encoder.free (ret);
  }

  PRINTMARK();
  Cystck_pushobject(S,newobj);
  return 1;

ERROR:
  Cystck_pop(S, separatorsItemBytes);
  Cystck_pop(S, separatorsKeyBytes);
  return -1;
}

Cystck_Object objToJSONFile(Py_State *S, Cystck_Object args, Cystck_Object kwargs)
{
  Cystck_Object data;
  Cystck_Object file;
  Cystck_Object string;
  Cystck_Object write;
  Cystck_Object argtuple;
  Cystck_Object write_result;

  PRINTMARK();

  if (!_PyArg_parseTuple (S,args, "OO", &data, &file))
  {
    return -1;
  }
  if (!Cystck_HasAttrString (S,file, "write"))
  {
    CystckErr_SetString (S, S->Cystck_TypeError, "expected file");
    return -1;
  }
  write = Cystck_GetAttr_String (S,file, "write");
  Cystck_pushobject(S, write);// pushcFunction
  stackStatus(S);
  if (Cystck_IsNULL(write))
  {
    Cystck_pop(S, write);
    return -1;

  }

  if (!Cystck_Callable_Check (S,write))
  {
    Cystck_pop(S, write);
    CystckErr_SetString (S, S->Cystck_TypeError, "expected file");
    return -1;
  }

  argtuple = Cystck_Tuple_Pack(S,1, data); //support type tuple
  Cystck_pushobject(S,argtuple);
  stackStatus(S);
  Cystck_Object  _objToJSON= Cystck_Import_ImportModule("ujson_cystck");
  Cystck_pushobject(S, _objToJSON);
  Cystck_Object func_dumps = Cystck_GetAttr_String (S,_objToJSON, "dumps");
  Cystck_pushobject(S, func_dumps);
  stackStatus(S);
  string = Cystck_Call_Object(S,func_dumps,argtuple);
  Cystck_pushobject(S, string);
  stackStatus(S);
  if (Cystck_IsNULL(string))
  {
    Cystck_pop(S,write);
    Cystck_pop(S,argtuple);
    return -1;
  }
  Cystck_pop(S,argtuple);

  argtuple = Cystck_Tuple_Pack(S, 1, string);
  Cystck_pushobject(S, argtuple);
  stackStatus(S);
  if (Cystck_IsNULL(argtuple))
  {
    Cystck_pop(S,write);
    return -1;
  }

  write_result = Cystck_Call_Object(S,write,argtuple);
  Cystck_pushobject(S, write_result);
  stackStatus(S);
  if (Cystck_IsNULL(write_result))
  {
    Cystck_pop(S,write);
    Cystck_pop(S,argtuple);
    return -1;
  }
  Cystck_pop(S,write_result);
  Cystck_pop(S,write);
  Cystck_pop(S,argtuple);
  //Cystck_pop(S, string);

  PRINTMARK();

  return 0;
}
Cystck_METH_DEF(objTo_JSON, "dumps",objToJSON, Cystck_METH_KEYWORDS, "Converts arbitrary object recursively into JSON. ");
Cystck_METH_DEF(objToJSON_encode, "encode",objToJSON, Cystck_METH_KEYWORDS, "Converts arbitrary object recursively into JSON. " );
Cystck_METH_DEF(objToJSON_File, "dump",objToJSONFile, Cystck_METH_KEYWORDS, "Converts arbitrary object recursively into JSON file. ");
