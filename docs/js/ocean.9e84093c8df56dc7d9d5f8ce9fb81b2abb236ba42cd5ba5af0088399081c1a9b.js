(() => {
  "use strict";
  const scene = document.querySelector(".heaven-scenery");
  if (!scene) return;
  const image = scene.querySelector("img");
  const canvas = scene.querySelector("canvas");
  const motion = matchMedia("(prefers-reduced-motion: reduce)");
  const gl = canvas.getContext("webgl", { alpha: false, antialias: false, depth: false, powerPreference: "low-power" });
  if (!gl) return; // The illustration remains visible without WebGL.

  const shader = (type, source) => {
    const result = gl.createShader(type);
    gl.shaderSource(result, source);
    gl.compileShader(result);
    if (!gl.getShaderParameter(result, gl.COMPILE_STATUS)) throw new Error("Ocean shader unavailable");
    return result;
  };
  let program;
  try {
    program = gl.createProgram();
    gl.attachShader(program, shader(gl.VERTEX_SHADER, `
      attribute vec2 position;
      varying vec2 uv;
      void main() {
        uv = vec2(position.x * .5 + .5, .5 - position.y * .5);
        gl_Position = vec4(position, 0., 1.);
      }
    `));
    gl.attachShader(program, shader(gl.FRAGMENT_SHADER, `
      precision mediump float;
      uniform sampler2D ocean;
      uniform vec2 crop;
      uniform float time;
      varying vec2 uv;
      void main() {
        vec2 p = (uv - .5) * crop + .5;
        float water = smoothstep(.555, .95, p.y);
        // Only the reflected sky moves; the horizon and clouds stay still.
        float swell = sin(p.y * 140. - time * .45 + sin(p.x * 8. + time * .16));
        float ripple = sin(p.y * 330. + time * .65 + p.x * 12.);
        p.x += (swell * .0012 + ripple * .00045) * water;
        p.y += sin(p.y * 190. - time * .3) * .00045 * water;
        vec3 color = texture2D(ocean, clamp(p, .001, .999)).rgb;
        float gleam = sin(p.y * 230. + p.x * 20. + time * .32);
        color += vec3(.008, .012, .018) * gleam * water;
        gl_FragColor = vec4(color, 1.);
      }
    `));
    gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) return;
  } catch { return; }
  gl.useProgram(program);
  const buffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1,-1, 1,-1, -1,1, -1,1, 1,-1, 1,1]), gl.STATIC_DRAW);
  const position = gl.getAttribLocation(program, "position");
  gl.enableVertexAttribArray(position);
  gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0);
  const crop = gl.getUniformLocation(program, "crop");
  const time = gl.getUniformLocation(program, "time");
  let ready = false, frame = 0, previous = 0;
  const draw = (now) => {
    if (!ready) return;
    gl.uniform1f(time, motion.matches ? 0 : now / 1000);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  };
  const resize = () => {
    if (!ready) return;
    const ratio = Math.min(devicePixelRatio || 1, 1.5);
    canvas.width = Math.round(innerWidth * ratio);
    canvas.height = Math.round(innerHeight * ratio);
    gl.viewport(0, 0, canvas.width, canvas.height);
    const scale = Math.max(innerWidth / image.naturalWidth, innerHeight / image.naturalHeight);
    gl.uniform2f(crop, innerWidth / (image.naturalWidth * scale), innerHeight / (image.naturalHeight * scale));
    draw(performance.now());
  };
  const animate = (now) => {
    frame = 0;
    if (document.hidden || motion.matches || !ready) return;
    if (now - previous >= 33) { draw(now); previous = now; }
    frame = requestAnimationFrame(animate);
  };
  const resume = () => {
    cancelAnimationFrame(frame);
    frame = 0;
    if (!ready) return;
    draw(performance.now());
    if (!document.hidden && !motion.matches) frame = requestAnimationFrame(animate);
  };
  const load = () => {
    if (ready || !image.naturalWidth) return;
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, gl.RGB, gl.UNSIGNED_BYTE, image);
    if (gl.getError() !== gl.NO_ERROR) return;
    ready = true;
    resize();
    canvas.hidden = false;
    resume();
  };
  image.addEventListener("load", load, { once: true });
  if (image.complete) load();
  addEventListener("resize", resize);
  document.addEventListener("visibilitychange", resume);
  motion.addEventListener("change", resume);
  canvas.addEventListener("webglcontextlost", () => {
    ready = false;
    cancelAnimationFrame(frame);
    canvas.hidden = true;
  });
})();
