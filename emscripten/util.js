// Copyright 2013 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Utility functions for emscripten html demo page.
//

try {
  Module['FS'] = FS;
} catch (e) {
  // If the emscripten functions have been optimized out, create local
  // implementations.
  var created = {};
  Module['FS'] = {};
  Module.FS['createDataFile'] = function(root, filename, data,
                                         can_read, can_write) {
    created[filename] = true;
    return Module.FS_createDataFile(root, filename, data,
                                    can_read, can_write);
  };
  Module.FS['analyzePath'] = function(path) {
    var ret = { exists: created[path] };
    return ret;
  };
}

function decode(filename) {
  Module.canvas = document.getElementById('output_canvas');
  start = new Date();
  var ret = Module.ccall('WebpToSDL', 'number', ['string'],[filename])
  end = new Date();
  speed_result = document.getElementById('timing');
  var decode_time = end - start;
  speed_result.innerHTML =
      'Timing:<p>decode: ' + decode_time +' ms.</p>';
}

function loadfile(filename) {
  var basename = filename.split('/').reverse()[0];
  path = Module.FS.analyzePath(basename);
  if (path != null && !path.exists) {
    document.getElementById('dl_progress').innerHTML = 'Downloading ...';
    var xhr = new XMLHttpRequest();
    xhr.open('GET', filename);
    xhr.responseType = 'arraybuffer';
    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4 && xhr.status == 200) {
        document.getElementById('dl_progress').innerHTML =
            'Download complete, displaying ...';
        var response = new Uint8Array(xhr.response);
        Module.FS.writeFile('/' + basename, response, { encoding : "binary" });
        decode(basename);
      }
    };
    xhr.send();
  } else {
    decode(basename);
  }
}
