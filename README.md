# stringhash_wasm

## Purpose
A WebAssembly version of the stringhash9a library from Waterslide so that it could be used withing web sites and Node.js.

## Running
This includes a stripped down serial version of stringhash9a hashtable that can be used for set membership operations much like a bloom filter.

You can either view the sh9.html page in a browser or run within node using
```console
node runsh9.js
```

## Modifications
if you make changes to stringhash9.h, you can compile it using:
```console
emcc stringhash9a.c -o sh9.js -s ALLOW_MEMORY_GROWTH=1  -s EXPORTED_FUNCTIONS="['_stringhash9a_create','_stringhash9a_set','_stringhash9a_check','_stringhash9a_destroy']" -s EXTRA_EXPORTED_RUNTIME_METHODS="['lengthBytesUTF8', 'stringToUTF8', 'writeArrayToMemory']"
```

### Passing data from javascript to stringhash9a (taken from runsh9.js)
```javascript
//  - if node.js then const Module = require('./sh9.js')

Module.onRuntimeInitialized = function() {

 //wait till the module is initialized before running
 var sh = Module._stringhash9a_create(100000);

 //allocate a memory space to share between C-HEAP and javascript
 //dataPtr will be the pointer to this allocated buffer
 var MAXBYTES = 64
 var dataPtr = Module._malloc(MAXBYTES);
 
 function stringhash9_set(str) {
    //copy string onto allocated memory buffer
    let strlen = Module.lengthBytesUTF8(str);
    strlen = (strlen <= MAXBYTES) ? strlen : MAXBYTES;
    Module.stringToUTF8(str, dataPtr, MAXBYTES);

    //call stringhash with pointer in C-code
    return Module._stringhash9a_set(sh, dataPtr, strlen));
  }
  
  //call function
  console.log("result " + stringhash9_set("mystring"));
  console.log("result " + stringhash9_set("mystring"));
}
````
