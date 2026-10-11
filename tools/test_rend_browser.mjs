#!/usr/bin/env node
// Exercise legacy Rend.h's actual GPU path through Chromium's CDP, without npm.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp, writeFile, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {createServer} from 'node:net';

const root = resolve(process.argv[2]);
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const port = () => new Promise(resolve => {
  const server = createServer();
  server.listen(0, '127.0.0.1', () => {
    const number = server.address().port;
    server.close(() => resolve(number));
  });
});
let server, browser, ws, temporary;
try {
  await writeFile(join(root, 'rend-smoke.html'), `<!doctype html>
<canvas id="canvas" width="160" height="120"></canvas>
<script src="rend-smoke.js"></script><script>
window.messages=[];window.errors=[];
createRendSmoke({print:s=>messages.push(s),printErr:s=>errors.push(s),
onAbort:s=>errors.push('ABORT '+s)}).then(m=>window.demo=m);
</script>`);
  temporary = await mkdtemp(join(tmpdir(), 'rend-browser-'));
  const [http, debug] = await Promise.all([port(), port()]);
  server = spawn('python3', ['-m', 'http.server', String(http), '--bind', '127.0.0.1', '--directory', root], {stdio:'ignore'});
  browser = spawn(process.env.CHROMIUM || '/usr/bin/chromium', [
    '--headless=new', '--no-sandbox', '--disable-dev-shm-usage', '--use-gl=angle',
    '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
    `--remote-debugging-port=${debug}`, `--user-data-dir=${join(temporary, 'profile')}`, 'about:blank',
  ], {stdio:'ignore'});
  let target;
  for (let n=0; n<100; ++n) {
    try { target=(await (await fetch(`http://127.0.0.1:${debug}/json/list`)).json()).find(t=>t.type==='page'); } catch {}
    if (target) break;
    await sleep(100);
  }
  assert(target, 'Chromium did not start');
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    ws.addEventListener('open', resolve, {once:true});
    ws.addEventListener('error', reject, {once:true});
  });
  let id=0;
  const pending=new Map(), exceptions=[];
  ws.addEventListener('message', event => {
    const message=JSON.parse(event.data);
    if (message.method==='Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails);
    if (message.id) {
      const response=pending.get(message.id);
      pending.delete(message.id);
      message.error ? response.reject(message.error) : response.resolve(message.result);
    }
  });
  const send=(method, params={}) => new Promise((resolve,reject) => {
    const number=++id;
    pending.set(number,{resolve,reject});
    ws.send(JSON.stringify({id:number,method,params}));
  });
  const evaluate=async expression => {
    const result=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});
    assert(!result.exceptionDetails,JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  await send('Runtime.enable');
  await send('Page.navigate',{url:`http://127.0.0.1:${http}/rend-smoke.html`});
  let ready=false;
  for (let n=0; n<200; ++n) {
    const state=await evaluate('({ready:!!window.demo,messages:window.messages||[],errors:window.errors||[]})');
    assert(!state.errors.some(line=>line.includes('REND_SMOKE_FAIL')||line.includes('ABORT')),JSON.stringify(state));
    if (state.ready) { ready=true; break; }
    await sleep(50);
  }
  assert(ready,'Test module did not initialize: '+JSON.stringify(await evaluate('({messages:window.messages,errors:window.errors,href:location.href})'))+' exceptions='+JSON.stringify(exceptions));
  assert((await evaluate('messages')).some(line=>line.includes('REND_SMOKE_PASS')),JSON.stringify(await evaluate('({messages,errors})')));
  // Resize through browser-owned canvas dimensions, not an emulated test backend.
  await evaluate('document.querySelector("canvas").width=247;document.querySelector("canvas").height=133');
  const resized=await evaluate(`(()=>{
    const ok=demo._rend_smoke_resize(247,133);
    const gl=document.querySelector('canvas').getContext('webgl2');
    const pixel=new Uint8Array(4);
    gl.readPixels(123,66,1,1,gl.RGBA,gl.UNSIGNED_BYTE,pixel);
    return {ok,pixel:Array.from(pixel),width:gl.drawingBufferWidth,height:gl.drawingBufferHeight};
  })()`);
  assert.equal(resized.ok,1,'GPU default target must follow canvas resize');
  assert.deepEqual(resized.pixel,[0,0,255,255],'Resized default framebuffer must contain presented pixels');
  assert.equal(resized.width,247);assert.equal(resized.height,133);
  assert.equal(await evaluate('demo._rend_smoke_finish()'),1,'Cleanup and renderer recreation failed');
  assert.deepEqual(exceptions,[]);
  console.log((await evaluate('messages')).join('\n'));
  console.log('PASS browser canvas resize, teardown and context recreation');
} finally {
  ws?.close();
  browser?.kill();
  server?.kill();
  if (temporary) await rm(temporary,{recursive:true,force:true,maxRetries:3,retryDelay:100});
}
