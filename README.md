# stringhash_wasm

## Purpose
A WebAssembly version of the stringhash9a library from [Waterslide](https://github.com/waterslidekid/waterslide)
so that it could be used within web sites and Node.js.  This repository has taken the [stringhash9a code written in C](https://github.com/waterslidekid/waterslide/blob/master/src/include/stringhash9a.h) from Waterslide, striped it down to the essential serial calls, and made it available for use in javascript using a compiled WebAssembly build.

## Running
[Try it](https://htmlpreview.github.io/?https://github.com/waterslidekid/stringhash_wasm/blob/master/sh9.html) in your browser.

This repository includes a stripped down serial version of stringhash9a hashtable that can be used for set membership operations much like a bloom filter.

You can either view the sh9.html page in a browser or run within node using
```console
node runsh9.js
```

## Modifications
First download the [emscripten emsdk](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html).

if you make changes to stringhash9a.c or stringhash9a.h, you can compile it using:
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
    return Module._stringhash9a_set(sh, dataPtr, strlen);
  }
  
  //call function
  console.log("result " + stringhash9_set("mystring"));
  console.log("result " + stringhash9_set("mystring"));
}
````
