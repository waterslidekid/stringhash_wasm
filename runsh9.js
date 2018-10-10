const Module = require('./sh9.js')
Module.print = console.log;

Module.onRuntimeInitialized = function() {

 var sh = Module._stringhash9a_create(100000);
 Module.print("pointer sh " + sh);

 var MAXBYTES = 64
 var dataPtr = Module._malloc(MAXBYTES);

 function stringhash9_set(str) {
   //copy string onto allocated memory buffer
   let strlen = Module.lengthBytesUTF8(str);
   strlen = (strlen <= MAXBYTES) ? strlen : MAXBYTES;
   Module.stringToUTF8(str, dataPtr, MAXBYTES);

   //call stringhash with pointer in C-code
   return Module._stringhash9a_set(sh, dataPtr, strlen);
 }

 Module.print("result: " + stringhash9_set("hello"));
 Module.print("result: " + stringhash9_set("hello1"));
 Module.print("result: " + stringhash9_set("hello"));
 Module.print("result: " + stringhash9_set("hello1"));

};
