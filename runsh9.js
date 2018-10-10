const Module = require('./sh9.js')
Module.print = console.log;

Module.onRuntimeInitialized = function() {

var sh = Module._stringhash9a_create(100000);
Module.print("pointer sh " + sh);

let MAXBYTES = 64

let dataPtr = Module._malloc(MAXBYTES);

let str = "foo"
let strlen = Module.lengthBytesUTF8(str);
strlen = (strlen <= MAXBYTES) ? strlen : MAXBYTES;
Module.stringToUTF8(str, dataPtr, MAXBYTES);
Module.print("set foo " + Module._stringhash9a_set(sh, dataPtr, strlen));
Module.print("set foo " + Module._stringhash9a_set(sh, dataPtr, strlen));
Module.print("set foo " + Module._stringhash9a_set(sh, dataPtr, strlen));

};
