#!/usr/bin/env node
// Real WebGL2 / Peak integration regression. Build the test module as documented
// in docs/rend-webgl2.md, then pass its directory (snake-test.js + .wasm).
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp,writeFile,readFile,mkdir,copyFile} from 'node:fs/promises';
import {tmpdir,homedir} from 'node:os';
import {join,resolve} from 'node:path';
import {createServer} from 'node:net';
import {inflateSync} from 'node:zlib';
const root=resolve(process.argv[2]||'/tmp');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const port=()=>new Promise(r=>{const s=createServer();s.listen(0,'127.0.0.1',()=>{const p=s.address().port;s.close(()=>r(p));});});
// Validate the composited screenshot as well as offscreen readback. A canvas
// can have correct dimensions and a ready module while displaying no frame.
function assertVisibleScene(png) {
  let offset=8,width,height,channels;const compressed=[];
  while(offset<png.length){const n=png.readUInt32BE(offset),kind=png.toString('ascii',offset+4,offset+8),data=png.subarray(offset+8,offset+8+n);
    if(kind==='IHDR'){width=data.readUInt32BE(0);height=data.readUInt32BE(4);assert.equal(data[8],8);channels=data[9]===2?3:data[9]===6?4:0;assert(channels);}
    if(kind==='IDAT')compressed.push(data);offset+=n+12;
  }
  const packed=inflateSync(Buffer.concat(compressed)),stride=width*channels;
  let previous=Buffer.alloc(stride),position=0,green=0,red=0;
  const paeth=(a,b,c)=>{const p=a+b-c,pa=Math.abs(p-a),pb=Math.abs(p-b),pc=Math.abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;};
  for(let y=0;y<height;y++){
    const filter=packed[position++],row=Buffer.from(packed.subarray(position,position+stride));position+=stride;
    for(let i=0;i<stride;i++){const a=i>=channels?row[i-channels]:0,b=previous[i],c=i>=channels?previous[i-channels]:0;
      row[i]=(row[i]+(filter===0?0:filter===1?a:filter===2?b:filter===3?Math.floor((a+b)/2):paeth(a,b,c)))&255;
    }
    for(let x=0;x<stride;x+=channels){const r=row[x],g=row[x+1],b=row[x+2];if(g>r*1.3&&g>b*1.3&&g>50)green++;if(r>g*1.8&&r>b*1.8&&r>70)red++;}
    previous=row;
  }
  assert(green>100&&red>20,`Canvas is not displaying Snake: ${green} green and ${red} red pixels`);
}
let server,browser,ws;
try {
  await writeFile(join(root,'snake-test.html'),`<!doctype html><canvas id="canvas" width="960" height="720" tabindex="0"></canvas>
<script src="snake-test.js"></script><script>
window.messages=[];window.errors=[];
createDemo({arguments:location.search?'--headless --frames 2 --ppm /snake.ppm'.split(' '):[],
print:s=>messages.push(s),printErr:s=>errors.push(s),onAbort:s=>errors.push(s)}).then(m=>window.demo=m);
</script>`);
  const temp=await mkdtemp(join(tmpdir(),'snake-browser-'));
  const http=await port(),debug=await port();
  server=spawn('python3',['-m','http.server',String(http),'--bind','127.0.0.1','--directory',root],{stdio:'ignore'});
  browser=spawn(process.env.CHROMIUM||'/usr/bin/chromium',['--headless=new','--no-sandbox','--disable-dev-shm-usage','--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader',`--remote-debugging-port=${debug}`,`--user-data-dir=${join(temp,'profile')}`,'about:blank'],{stdio:'ignore'});
  let target;
  for(let n=0;n<100;++n){try {target=(await(await fetch(`http://127.0.0.1:${debug}/json/list`)).json()).find(t=>t.type==='page');if(target)break;}catch{}await sleep(100);}
  assert(target,'Chromium did not start');
  ws=new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
  let id=0;const pending=new Map();const exceptions=[];
  ws.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.method==='Runtime.exceptionThrown')exceptions.push(m.params.exceptionDetails);if(m.id){const p=pending.get(m.id);pending.delete(m.id);m.error?p.reject(m.error):p.resolve(m.result);}});
  const send=(method,params={})=>new Promise((resolve,reject)=>{const n=++id;pending.set(n,{resolve,reject});ws.send(JSON.stringify({id:n,method,params}));});
  const evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
  const wait=async expression=>{for(let n=0;n<200;++n){if(await evaluate(expression))return;await sleep(50);}throw Error('Timed out: '+expression+' '+JSON.stringify(await evaluate('window.errors')));};
  await send('Runtime.enable');await send('Page.enable');
  await send('Page.navigate',{url:`http://127.0.0.1:${http}/snake-test.html?headless`});
  await wait('window.demo && demo._snake_test_state(5)===1');
  const ppm=Buffer.from(await evaluate('Array.from(demo.FS.readFile("/snake.ppm"))'));
  await writeFile(join(temp,'web.ppm'),ppm);
  const offset=ppm.indexOf(Buffer.from('\n255\n'))+5,pixels=ppm.subarray(offset),colors=new Set();
  let green=0,red=0;
  for(let i=0;i<pixels.length;i+=3){const [r,g,b]=pixels.subarray(i,i+3);colors.add(`${r},${g},${b}`);if(g>r*1.3&&g>b*1.3&&g>50)green++;if(r>g*1.8&&r>b*1.8&&r>70)red++;}
  assert(colors.size>=15&&green>1000&&red>100,`Missing scene: ${colors.size} colors, ${green} green, ${red} red`);
  if(process.argv[3]){
    const native=await readFile(process.argv[3]);const start=native.indexOf(Buffer.from('\n255\n'))+5;
    assert.equal(native.length-start,pixels.length);
    let different=0;
    for(let i=0;i<pixels.length;i+=3)if(Math.max(...[0,1,2].map(c=>Math.abs(pixels[i+c]-native[start+i+c])))>8)different++;
    assert(different/(pixels.length/3)<.02,`Native/WebGL2 scene mismatch: ${different} pixels`);
    console.log(`PASS native/WebGL2 fixed-frame comparison: ${different} pixels differ >8/255`);
  }
  assert.deepEqual(await evaluate('errors'),[]);
  console.log(`PASS headless WebGL2 depth/instancing/readback/cleanup: ${colors.size} colors, ${green} green, ${red} red`);
  await send('Page.navigate',{url:`http://127.0.0.1:${http}/snake-test.html`});
  await wait('window.demo && demo._snake_test_state(0)>2');
  await evaluate('document.querySelector("canvas").focus()');
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowDown',code:'ArrowDown',windowsVirtualKeyCode:40});
  await send('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowDown',code:'ArrowDown',windowsVirtualKeyCode:40});
  await wait('demo._snake_test_state(2)===1');
  assert.equal(await evaluate('demo._snake_test_resize()'),1,'Peak resizes software storage before exposing new dimensions');
  await evaluate('document.querySelector("canvas").width=800;document.querySelector("canvas").height=600');
  const frame=await evaluate('demo._snake_test_state(0)');
  await wait(`demo._snake_test_state(0)>${frame+3}`);
  await writeFile(join(temp,'interactive.png'),Buffer.from((await send('Page.captureScreenshot')).data,'base64'));
  await wait('demo._snake_test_state(3)===0');
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:' ',code:'Space',windowsVirtualKeyCode:32});
  await send('Input.dispatchKeyEvent',{type:'keyUp',key:' ',code:'Space',windowsVirtualKeyCode:32});
  await wait('demo._snake_test_state(3)===1 && demo._snake_test_state(1)===1');
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
  await wait('demo._snake_test_state(5)===1');
  assert.deepEqual(await evaluate('errors'),[]);
  const site=resolve(process.env.GODSTACK_SITE || join(homedir(),'rsr/src/site'));
  const published=join(site,'static/demos/snake');
  if (process.env.GODSTACK_SKIP_PUBLISHED === '1') {
    console.log('SKIP published runner: external site checkout not supplied');
  } else {
    await mkdir(join(root,'published'),{recursive:true});
    for(const file of ['demo.js','run.html'])await copyFile(join(published,file),join(root,'published',file));
    await send('Page.navigate',{url:`http://127.0.0.1:${http}/published/run.html`});
    await wait(`document.documentElement?.dataset.runState==='ready' && !!document.getElementById('canvas')?.getContext('webgl2')`);
    await sleep(500);
    assert.equal(await evaluate('document.documentElement.dataset.runState'),'ready');
    const publishedScreenshot=Buffer.from((await send('Page.captureScreenshot')).data,'base64');
    await writeFile(join(temp,'published.png'),publishedScreenshot);
    assertVisibleScene(publishedScreenshot);
    console.log('PASS published single-file WebAssembly runner');
  }
  assert.deepEqual(exceptions,[]);
  console.log('PASS Peak keyboard steering, browser canvas resize, crash/restart, Escape, resource teardown');
  console.log('Artifacts: '+temp);
} finally {ws?.close();browser?.kill();server?.kill();}
