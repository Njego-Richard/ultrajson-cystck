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
#include "../lib/ultrajson.h"
#include "ujson.h"


//#define PRINTMARK() fprintf(stderr, "%s: MARK(%d)\n", __FILE__, __LINE__)
#define PRINTMARK()

static void Object_objectAddKey(void *prv, JSOBJ obj, JSOBJ name, JSOBJ value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  Cystck_Object _name = Cystck_FromVoid(name);
  Cystck_Object _value = Cystck_FromVoid(value);
  Cystck_pushobject(S,_name);
  Cystck_pushobject(S,_value);
  CystckDict_SetItem(S,_obj,_name,_value);
  Cystck_pushobject(S,_obj);
  Cystck_pop(S,_name);
  Cystck_pop(S,_value);
  return;
}

static void Object_arrayAddItem(void *prv, JSOBJ obj, JSOBJ value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  Cystck_Object _value = Cystck_FromVoid(value);
  Cystck_pushobject(S,_value);
  CystckList_Append(S,_obj, _value);
  Cystck_pushobject(S,_obj);
  Cystck_pop(S,_value);
  return;
}

/*
Check that Py_UCS4 is the same as JSUINT32, else Object_newString will fail.
Based on Linux's check in vbox_vmmdev_types.h.
This should be replaced with
  _Static_assert(sizeof(Py_UCS4) == sizeof(JSUINT32));
when C11 is made mandatory (CPython 3.11+, PyPy ?).
*/
typedef char assert_py_ucs4_is_jsuint32[1 - 2*!(sizeof(Py_UCS4) == sizeof(JSUINT32))];

static JSOBJ Object_newString(void *prv, JSUINT32 *start, JSUINT32 *end)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckUnicode_FromKindAndData(S, PyUnicode_4BYTE_KIND, (Py_UCS4 *) start, (end - start));
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newTrue(void *prv)
{
  Py_State *S = (Py_State *)prv;
  Cystck_Object res = Cystck_Dup(S,S->Cystck_True);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newFalse(void *prv)
{
  Py_State *S = (Py_State *)prv;
  Cystck_Object res = Cystck_Dup(S,S->Cystck_False);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newNull(void *prv)
{
  Py_State *S = (Py_State *)prv;
  Cystck_Object res = Cystck_Dup(S,S->Cystck_None);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newNaN(void *prv)
{
    Py_State *S = (Py_State *)prv;
    Cystck_Object res = CystckFloat_FromDouble(S,Py_NAN);
    return Cystck_AsVoidP(res);
}

static JSOBJ Object_newPosInf(void *prv)
{
    Py_State *S = (Py_State *)prv;
    Cystck_Object res = CystckFloat_FromDouble(S,Py_HUGE_VAL);
    return Cystck_AsVoidP(res);
}

static JSOBJ Object_newNegInf(void *prv)
{
    Py_State *S = (Py_State *)prv;
    Cystck_Object res = CystckFloat_FromDouble(S,-Py_HUGE_VAL);
    return Cystck_AsVoidP(res);
}

static JSOBJ Object_newObject(void *prv)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckDict_New(S);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newArray(void *prv)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckList_New(S,0);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newInteger(void *prv, JSINT32 value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckLong_FromLong(S,(long) value);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newLong(void *prv, JSINT64 value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckLong_FromLongLong(S, value);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newUnsignedLong(void *prv, JSUINT64 value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckLong_FromUnsignedLong(S,value);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newIntegerFromString(void *prv, char *value, size_t length)
{
  // PyLong_FromString requires a NUL-terminated string in CPython, contrary to the documentation: https://github.com/python/cpython/issues/59200
  char *buf = malloc(length + 1);
  memcpy(buf, value, length);
  buf[length] = '\0';
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckLong_FromString(S, buf, NULL, 10);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static JSOBJ Object_newDouble(void *prv, double value)
{
  Py_State *S =  (Py_State *) prv;
  Cystck_Object res = CystckFloat_FromDouble(S,value);
  Cystck_pushobject(S,res);
  return Cystck_AsVoidP(res);
}

static void Object_releaseObject(void *prv, JSOBJ obj)
{
  Py_State *S = (Py_State *)prv;
  Cystck_Object _obj = Cystck_FromVoid(obj);
  Cystck_pop(S, _obj);
}

static char *g_kwlist[] = {"obj", NULL};

Cystck_Object JSONToObj(Py_State *S, Cystck_Object args, Cystck_Object kwargs)
{
  Cystck_Object sarg;
  Cystck_Object arg;
  JSONObjectDecoder decoder =
  {
    Object_newString,
    Object_objectAddKey,
    Object_arrayAddItem,
    Object_newTrue,
    Object_newFalse,
    Object_newNull,
    Object_newNaN,
    Object_newPosInf,
    Object_newNegInf,
    Object_newObject,
    Object_newArray,
    Object_newInteger,
    Object_newLong,
    Object_newUnsignedLong,
    Object_newIntegerFromString,
    Object_newDouble,
    Object_releaseObject,
    malloc,
    free,
    realloc
  };

  decoder.prv = S;

  if (!_PyArg_parseTupleAndKeywords(S, args, kwargs, "O", g_kwlist, &arg))
  {
      return -1;
  }

  if (CystckBytes_Check(S,arg))
  {
      sarg = arg;
      Cystck_pushobject(S,sarg);
  }
  else
  if (CystckUnicode_Check(S, arg))
  {
    sarg = CystckUnicode_AsEncodedString(S, arg, NULL, "surrogatepass");
    Cystck_pushobject(S,sarg);
    if (Cystck_IsNULL(sarg) )
    {
      //Exception raised above us by codec according to docs
      return -1;
    }
  }
  else
  {
    CystckErr_SetString (S, S->Cystck_TypeError, "Expected String or Unicode");
    return -1;
  }

  decoder.errorStr = NULL;
  decoder.errorOffset = NULL;

  decoder.s2d = NULL;
  dconv_s2d_init(&decoder.s2d, DCONV_S2D_ALLOW_TRAILING_JUNK, 0.0, 0.0, "Infinity", "NaN");
  void *ret;
  ret = JSON_DecodeObject(&decoder, 
                                  CystckBytes_AS_STRING(S, sarg), 
                                  CystckBytes_GET_SIZE(S, sarg));

  Cystck_Object _ret = Cystck_FromVoid(ret);
  Cystck_pushobject(S, _ret);
  dconv_s2d_free(&decoder.s2d);

  if (sarg != arg)
  {
    Cystck_pop(S,sarg);
  }

  if (decoder.errorStr)
  {
    /*
    FIXME: It's possible to give a much nicer error message here with actual failing element in input etc*/

    CystckErr_SetString (S, S->Cystck_ValueError, decoder.errorStr);

    if (Cystck_IsNULL(_ret))
    {
        Cystck_pop(S,_ret);
    }

    return -1;
  }
  Cystck_pushobject(S,_ret);
  return 1;
}

Cystck_Object JSONFileToObj(Py_State *S, Cystck_Object args, Cystck_Object kwargs)
{
  Cystck_Object read;
  Cystck_Object string;
  //PyObject *result;
  Cystck_Object _result;
  Cystck_Object file = 0;
  Cystck_Object argtuple;

  if (!_PyArg_parseTuple (S,args, "O", &file))
  {
    return -1;
  }

  if (!Cystck_HasAttrString (S,file, "read"))
  {
    CystckErr_SetString (S, S->Cystck_TypeError, "expected file");
    return -1;
  }

  read = Cystck_GetAttr_String (S,file, "read");
  Cystck_pushobject(S,read);
  if (!Cystck_Callable_Check (S,read)) {
    Cystck_pop(S,read);
    CystckErr_SetString (S, S->Cystck_TypeError, "expected file");
    return -1;
  }

  string = Cystck_Call_Object (S,read, 0);
  Cystck_pushobject(S,string);
  Cystck_pop(S,read);

  if (Cystck_IsNULL(string))
  {
    return -1;
  }
  argtuple = Cystck_Tuple_Pack(S,1, string);
  Cystck_pushobject(S,argtuple);

  Cystck_Object  _JSONToObj= Cystck_Import_ImportModule("ujson_cystck");
  Cystck_Object func_loads = Cystck_GetAttr_String (S,_JSONToObj, "loads");
  Cystck_pushobject(S, func_loads);
  Cystck_Object result = Cystck_Call_Object(S,func_loads,argtuple);

  Cystck_pop(S,argtuple);
  Cystck_pop(S,string);

  if (Cystck_IsNULL(result)) {
    return -1;
  }

  Cystck_pushobject(S, result);
  return 1;
}

Cystck_METH_DEF(JSONTo_Obj, "loads",JSONToObj, Cystck_METH_KEYWORDS, "Converts JSON as string to dict object structure. " );
Cystck_METH_DEF(JSONToObj_decode, "decode",JSONToObj, Cystck_METH_KEYWORDS, "Converts JSON as string to dict object structure. " );
Cystck_METH_DEF(JSONFileTo_Obj, "load",JSONFileToObj, Cystck_METH_KEYWORDS, "Converts JSON as file to dict object structure. " );
