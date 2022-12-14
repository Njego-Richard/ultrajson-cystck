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

#include  "../../include/Cystck.h"
#include "version_template.h"
#include "ujson.h"

/* objToJSON */
extern CyMethodDef objTo_JSON;
extern CyMethodDef objToJSON_encode;

/* JSONToObj */
extern CyMethodDef JSONTo_Obj;
extern CyMethodDef JSONToObj_decode;
/* objToJSONFile */
extern CyMethodDef objToJSON_File;
/* JSONFileToObj */
extern CyMethodDef JSONFileTo_Obj;

PyObject* JSONDecodeError;


#define ENCODER_HELP_TEXT "Use ensure_ascii=false to output UTF-8. " \
    "Set encode_html_chars=True to encode < > & as unicode escape sequences. "\
    "Set escape_forward_slashes=False to prevent escaping / characters." \
    "Set allow_nan=False to raise an exception when NaN or Infinity would be serialized." \
    "Set reject_bytes=True to raise TypeError on bytes."

static CyMethodDef *module_defines[] = {
    &objToJSON_File,
    &JSONFileTo_Obj,
    &JSONTo_Obj,
    &objTo_JSON,
    &JSONToObj_decode,
    &objToJSON_encode,
    NULL
};

typedef struct {
  PyObject *type_decimal;
} modulestate;



static CyModuleDef moduledef = {
  "ujson_Cystck",
  0,
  -1,
  module_defines,
};


/* Used in objToJSON.c */
int object_is_decimal_type(Py_State *S, Cystck_Object obj)
{
  Cystck_Object module = Cystck_Import_ImportModule("decimal");
  Cystck_pushobject(S, module);
  if (Cystck_IsNULL(module)) {
    Cystck_Err_Clear(S);
    return 0;
  }
  Cystck_Object type_decimal = Cystck_GetAttr_String(S, module, "Decimal");
  Cystck_pushobject(S, type_decimal);
  if (Cystck_IsNULL(type_decimal)) {
    Cystck_pop(S,module);
    Cystck_Err_Clear(S);
    return 0;
  }
  int result = Cystck_IsInstance(S, obj, type_decimal);
  if (result == -1) {
    Cystck_pop(S,module);
    Cystck_pop(S,type_decimal);
    Cystck_Err_Clear(S);
    return 0;
  }
  return result;
}

CyMODINIT_FUNC (ujson_cystck)
CyInit_ujson_cystck(Py_State *Py_state)
{
    Cystck_Object module;
    module = CystckModule_Create(Py_state,&moduledef);
    Cystck_pushobject(Py_state, module);
    if (Cystck_IsNULL(module))
    {
      Cystck_pop(Py_state,module);
      return 0;
    }
    Cystck_Object version_string;
    version_string = CystckUnicode_FromString(Py_state, UJSON_VERSION);
    if (Cystck_IsNULL(version_string))
    {
      return 0;
    }
    Cystck_SetAttrString(Py_state, module, "__version__", version_string);
   return module; 
}