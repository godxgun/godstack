#!/usr/bin/env node
// Dependency-free browser smoke/interaction checks for a rendered Hugo site.
// Usage: node tools/test_site_browser.mjs PATH-TO-HUGO-OUTPUT
import { spawn } from 'node:child_process';
import { mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { createServer } from 'node:net';

const root = resolve(process.argv[2] ?? '');
const captureArg = process.argv.indexOf('--capture-dir');
const captureDir = captureArg >= 0 ? resolve(process.argv[captureArg + 1] ?? '') : null;
if (!process.argv[2] || (captureArg >= 0 && !process.argv[captureArg + 1])) {
  console.error('Usage: node tools/test_site_browser.mjs PATH-TO-HUGO-OUTPUT [--capture-dir DIR] [--skip-demos]');
  process.exit(2);
}
const chromium = process.env.CHROMIUM ?? '/usr/bin/chromium';
const failures = [];
const checks = [];
const check = (ok, message) => { checks.push(message); if (!ok) failures.push(message); };
const sleep = ms => new Promise(r => setTimeout(r, ms));

const reservePort = () => new Promise((resolvePort, reject) => {
  const s = createServer();
  s.once('error', reject);
  s.listen(0, '127.0.0.1', () => { const p = s.address().port; s.close(() => resolvePort(p)); });
});

class CDP {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.nextId = 1;
    this.pending = new Map();
    this.ready = new Promise((resolveReady, reject) => {
      this.ws.addEventListener('open', resolveReady, { once: true });
      this.ws.addEventListener('error', reject, { once: true });
    });
    this.ws.addEventListener('message', event => {
      const msg = JSON.parse(event.data);
      if (msg.id && this.pending.has(msg.id)) {
        const { resolve, reject } = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        if (msg.error) reject(new Error(msg.error.message)); else resolve(msg.result);
      }
    });
  }
  send(method, params = {}, sessionId = null) {
    const id = this.nextId++;
    return new Promise((resolveSend, reject) => {
      this.pending.set(id, { resolve: resolveSend, reject });
      this.ws.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }));
    });
  }
  close() { this.ws.close(); }
}

let server, browser, cdp, temp;
try {
  temp = await mkdtemp(join(tmpdir(), 'godstack-browser-'));
  const port = await reservePort();
  server = spawn('python3', ['-m', 'http.server', String(port), '--bind', '127.0.0.1', '--directory', root], { stdio: 'ignore' });
  const base = `http://127.0.0.1:${port}`;
  let ready = false;
  for (let i = 0; i < 60; i++) {
    try { if ((await fetch(`${base}/`)).ok) { ready = true; break; } } catch {}
    await sleep(100);
  }
  if (!ready) throw new Error(`static server failed for ${root}`);

  const debugPort = await reservePort();
  browser = spawn(chromium, ['--headless=new', '--no-sandbox', '--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--disable-dev-shm-usage', `--remote-debugging-port=${debugPort}`, `--user-data-dir=${join(temp, 'profile')}`, 'about:blank'], { stdio: 'ignore' });
  let target;
  for (let i = 0; i < 80; i++) {
    try {
      const targets = await (await fetch(`http://127.0.0.1:${debugPort}/json/list`)).json();
      target = targets.find(t => t.type === 'page');
      if (target) break;
    } catch {}
    await sleep(100);
  }
  if (!target) throw new Error('Chromium DevTools endpoint did not become available');

  cdp = new CDP(target.webSocketDebuggerUrl);
  await cdp.ready;
  await cdp.send('Page.enable');
  await cdp.send('Runtime.enable');
  await cdp.send('Log.enable');
  await cdp.send('Target.setDiscoverTargets', { discover: true });
  const evaluate = async expression => {
    const result = await cdp.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
    if (result.exceptionDetails) throw new Error(result.exceptionDetails.exception?.description ?? result.exceptionDetails.text ?? 'page evaluation failed');
    return result.result?.value;
  };
  const navigate = async path => {
    await cdp.send('Page.navigate', { url: `${base}${path}` });
    for (let i = 0; i < 100; i++) {
      const state = await evaluate('document.readyState');
      if (state === 'complete') return;
      await sleep(50);
    }
    throw new Error(`navigation timeout: ${path}`);
  };
  const waitFor = async (expression, description, attempts = 200, interval = 100) => {
    for (let i = 0; i < attempts; i++) {
      if (await evaluate(expression)) return true;
      await sleep(interval);
    }
    throw new Error(`timed out waiting for ${description}`);
  };
  const screenshot = async name => {
    const result = await cdp.send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
    const fs = await import('node:fs/promises');
    await fs.writeFile(join(temp, name), Buffer.from(result.data, 'base64'));
  };
  const waitForFrameTarget = async path => {
    for (let i = 0; i < 120; i++) {
      const targets = await cdp.send('Target.getTargets');
      const target = targets.targetInfos?.find(info => info.type === 'iframe' && new URL(info.url).pathname === path);
      if (target) return target;
      await sleep(50);
    }
    throw new Error(`timed out waiting for iframe target ${path}`);
  };
  const attachFrame = async path => {
    const target = await waitForFrameTarget(path);
    const { sessionId } = await cdp.send('Target.attachToTarget', { targetId: target.targetId, flatten: true });
    await cdp.send('Runtime.enable', {}, sessionId);
    const frameEvaluate = async expression => {
      const response = await cdp.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, sessionId);
      if (response.exceptionDetails) throw new Error(response.exceptionDetails.exception?.description ?? response.exceptionDetails.text ?? 'iframe evaluation failed');
      return response.result?.value;
    };
    const frameWaitFor = async (expression, description, attempts = 200, interval = 100) => {
      for (let i = 0; i < attempts; i++) {
        if (await frameEvaluate(expression)) return true;
        await sleep(interval);
      }
      throw new Error(`timed out waiting for ${description}`);
    };
    return { target, sessionId, evaluate: frameEvaluate, waitFor: frameWaitFor };
  };
  const canvasStateExpression = `(() => {const c=document.querySelector('#canvas');if(!c||c.hidden||c.width<1||c.height<1)return null;const d=c.getContext('2d')?.getImageData(0,0,c.width,c.height).data;if(!d)return null;let h=2166136261,painted=0,colors=new Set();for(let i=0;i<d.length;i+=4){h=Math.imul(h^d[i],16777619);h=Math.imul(h^d[i+1],16777619);h=Math.imul(h^d[i+2],16777619);h=Math.imul(h^d[i+3],16777619);if(d[i+3]){painted++;colors.add((d[i]<<16)|(d[i+1]<<8)|d[i+2]);}}return {width:c.width,height:c.height,hash:(h>>>0).toString(16),painted,colors:colors.size};})()`;
  const captureCanvas = async demo => {
    if (!captureDir) return;
    const dataUrl = await evaluate(`document.querySelector('#canvas')?.toDataURL('image/png')`);
    if (typeof dataUrl !== 'string' || !dataUrl.startsWith('data:image/png;base64,')) throw new Error(`${demo} canvas did not provide a PNG capture`);
    await mkdir(captureDir, { recursive: true });
    await writeFile(join(captureDir, `${demo}-demo.png`), Buffer.from(dataUrl.slice('data:image/png;base64,'.length), 'base64'));
    console.log(`Captured actual ${demo} browser canvas to ${join(captureDir, `${demo}-demo.png`)}`);
  };
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  await cdp.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
  await navigate('/');
  check((await evaluate('document.title')).length > 0, 'home page loads with a document title');
  check(await evaluate('!!document.querySelector("main")'), 'home page contains main landmark');
  const colors = await evaluate(`(() => {const s=getComputedStyle(document.body); return {fg:s.color,bg:s.backgroundColor==='rgba(0, 0, 0, 0)'?getComputedStyle(document.documentElement).backgroundColor:s.backgroundColor};})()`);
  const rgb = value => value.match(/[\d.]+/g)?.slice(0, 3).map(Number);
  const luminance = value => value.map(c => { c /= 255; return c <= .04045 ? c / 12.92 : ((c + .055) / 1.055) ** 2.4; }).reduce((sum, c, i) => sum + c * [.2126, .7152, .0722][i], 0);
  const fg = rgb(colors.fg), bg = rgb(colors.bg);
  const contrast = fg && bg ? (Math.max(luminance(fg), luminance(bg)) + .05) / (Math.min(luminance(fg), luminance(bg)) + .05) : 0;
  check(contrast >= 4.5, `body text meets WCAG AA contrast (measured ${contrast.toFixed(2)}:1)`);

  const interactions = await evaluate(`(() => {
    const links = [...document.querySelectorAll('a[href]')];
    const expected = ['/docs/', '/guides/getting-started/'];
    const navigation = expected.every(x => links.some(a => a.getAttribute('href')?.includes(x)));
    const search = document.querySelector('input[type=search], input[placeholder*="earch" i], [role=search] input');
    return { navigation, search: !!search };
  })()`);
  check(interactions.navigation, 'primary links navigate to docs and getting-started');
  check(await evaluate(`[...document.querySelectorAll('.topnav a')].some(a=>/^symbols(?: vault)?$/i.test(a.textContent.trim())&&a.getAttribute('href')==='/symbols/')`), 'top navigation exposes the Symbols vault link');
  check(interactions.search, 'search input is available on the home page');
  check(await evaluate(`document.querySelector('link[rel="icon"]')?.getAttribute('href') === '/favicon.svg'`), 'site exposes the SVG favicon');
  check(await evaluate(`document.querySelectorAll('.product-card h3 .library-icon').length === 9 && [...document.querySelectorAll('.product-card h3 .library-icon')].every(icon=>icon.getAttribute('aria-hidden')==='true' && icon.getAttribute('viewBox')==='0 0 24 24')`), 'all nine library cards have decorative SVG icons');

  const overflow = async width => {
    await cdp.send('Page.setDeviceMetricsOverride', { width, height: 900, deviceScaleFactor: 1, mobile: width < 600 });
    await sleep(150);
    return evaluate('({viewport:innerWidth, document:document.documentElement.scrollWidth, main:!!document.querySelector("main")})');
  };
  for (const width of [390, 768, 1440]) {
    const layout = await overflow(width);
    check(layout.document <= layout.viewport + 1, `layout has no horizontal page overflow at ${width}px`);
    if (layout.document > layout.viewport + 1) {
      console.error('Overflow diagnostics:', await evaluate(`JSON.stringify([...document.querySelectorAll('body *')].filter(e => e.getBoundingClientRect().right > innerWidth + 1).map(e => ({tag:e.tagName,class:e.className,right:Math.round(e.getBoundingClientRect().right),width:Math.round(e.getBoundingClientRect().width)})).slice(0,25))`));
    }
    if (width === 390) check(await evaluate('getComputedStyle(document.querySelector(".search-open")).fontSize !== "0px" && getComputedStyle(document.querySelector(".search-open kbd")).display === "none"'), 'mobile search keeps its visible label and hides the keyboard shortcut');
    await screenshot(`home-${width}.png`);
  }

  // Check the actual shader clock; a static screenshot cannot prove motion.
  await cdp.send('Page.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
  const oceanState = `(() => {const canvas=document.querySelector('.heaven-scenery canvas');const gl=canvas?.getContext('webgl');const program=gl?.getParameter(gl.CURRENT_PROGRAM);return {visible:!!canvas&&!canvas.hidden,webgl:!!gl,time:program?gl.getUniform(program,gl.getUniformLocation(program,'time')):null};})()`;
  await waitFor(`document.querySelector('.heaven-scenery img')?.complete`, 'ocean image loading');
  const stillOcean = await evaluate(oceanState);
  if (stillOcean.webgl) {
    check(stillOcean.visible && stillOcean.time === 0, 'ocean shader renders a still frame with reduced motion');
    await cdp.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'no-preference' }] });
    await sleep(500);
    const firstOcean = await evaluate(oceanState);
    await sleep(500);
    const nextOcean = await evaluate(oceanState);
    check(firstOcean.visible && nextOcean.time > firstOcean.time, 'ocean shader clock advances when motion is allowed');
    await cdp.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
    await waitFor(`matchMedia('(prefers-reduced-motion: reduce)').matches`, 'ocean reduced-motion change');
    await sleep(250);
    const pausedOcean = await evaluate(oceanState);
    await sleep(500);
    check((await evaluate(oceanState)).time === pausedOcean.time, 'ocean stops advancing when reduced motion is enabled');
  } else {
    check(await evaluate(`document.querySelector('.heaven-scenery img').naturalWidth > 0`), 'ocean image remains available without WebGL');
  }

  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  check(await evaluate('getComputedStyle(document.documentElement).colorScheme === "dark"'), 'Heaven uses its fixed dark palette');
  await cdp.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
  const motion = await evaluate('({matches:matchMedia("(prefers-reduced-motion: reduce)").matches,scroll:getComputedStyle(document.documentElement).scrollBehavior,duration:getComputedStyle(document.body).transitionDuration})');
  check(motion.matches && motion.scroll === 'auto' && (parseFloat(motion.duration) === 0 || parseFloat(motion.duration) <= 0.01), 'reduced-motion preference disables smooth scroll and shortens transitions');

  await navigate('/docs/');
  check(await evaluate('!!document.querySelector("main") && document.querySelectorAll("a[href]").length > 0'), 'documentation index renders navigable content');
  check(await evaluate('!document.querySelector(".sidebar")'), 'documentation landing page avoids duplicate sidebar lists');
  await navigate('/symbols/');
  check(await evaluate('!!document.querySelector("main") && !document.querySelector(".sidebar") && document.querySelectorAll("a[href]").length > 0'), 'symbol-vault landing page renders without a duplicate sidebar');
  await navigate('/docs/cast/');
  await cdp.send('Page.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
  const menu = await evaluate(`(() => {const b=document.querySelector('.menu-toggle'); if(!b)return false; b.click(); return b.getAttribute('aria-expanded')==='true' && document.querySelector('.sidebar')?.classList.contains('is-open');})()`);
  check(menu, 'mobile menu opens and updates its expanded state');
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  await navigate('/docs/cast/');
  for (const width of [390, 768, 1440]) {
    const layout = await overflow(width);
    check(layout.document <= layout.viewport + 1, `article has no horizontal page overflow at ${width}px`);
  }
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Tab', code: 'Tab' });
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Tab', code: 'Tab' });
  check(await evaluate(`(() => {const e=document.activeElement;const s=getComputedStyle(e);return e!==document.body && s.outlineStyle!=='none' && parseFloat(s.outlineWidth)>0;})()`), 'keyboard focus is visible on the first tab stop');
  await cdp.send('Browser.grantPermissions', { origin: base, permissions: ['clipboardReadWrite', 'clipboardSanitizedWrite'] });
  const copy = await evaluate(`(async()=>{const pre=document.querySelector('pre'); const b=pre?.closest('.code-block')?.querySelector('.copy-code'); if(!b)return {missing:true}; const expected=pre.querySelector('code')?.innerText || pre.innerText; b.click(); await new Promise(r=>setTimeout(r,150)); let actual=false; try{actual=(await navigator.clipboard.readText())===expected}catch{} if(actual)return {label:b.textContent,actual:true,copied:expected}; Object.defineProperty(navigator,'clipboard',{configurable:true,value:{writeText:async text=>{window.__copiedText=text;}}}); b.click(); await new Promise(r=>setTimeout(r,150)); return {label:b.textContent,actual:false,copied:window.__copiedText};})()`);
  check(copy?.label === 'Copied' && typeof copy.copied === 'string' && copy.copied.length > 0, `code copy button confirms clipboard interaction (${copy?.actual ? 'real clipboard' : 'browser clipboard unavailable; API stub'})`);
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'k', code: 'KeyK', modifiers: 2 });
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'k', code: 'KeyK', modifiers: 2 });
  check(await evaluate('document.querySelector(".search-dialog")?.open === true && document.activeElement.id === "search-input"'), 'Ctrl+K opens search and focuses its input');
  for (let i = 0; i < 6; i++) {
    await cdp.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Tab', code: 'Tab' });
    await cdp.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Tab', code: 'Tab' });
  }
  check(await evaluate('(() => {const d=document.querySelector(".search-dialog");return d?.open && d.contains(document.activeElement);})()'), 'search modal keeps keyboard focus inside the dialog');
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Escape', code: 'Escape' });
  await cdp.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Escape', code: 'Escape' });
  await evaluate(`(() => {const b=document.querySelector('.search-open'); b?.click(); const i=document.querySelector('#search-input'); if(i){i.value='CastAst'; i.dispatchEvent(new Event('input',{bubbles:true}));} })()`);
  await waitFor(`!!document.querySelector('.search-results a[href*="/symbols/cast/#castast"]')`, 'symbol search results');
  check(await evaluate(`!!document.querySelector('.search-results a[href*="/symbols/cast/#castast"]')`), 'search for a C symbol lands on its anchored symbol-vault entry');
  await navigate('/guides/getting-started/');
  check(await evaluate('!!document.querySelector("main") && document.querySelector("h1")'), 'getting-started guide renders a main heading');

  if (!process.argv.includes('--skip-demos')) {
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  await cdp.send('Emulation.setEmulatedMedia', { features: [] });
  await navigate('/demos/');
  check(await evaluate('!!document.querySelector("main") && document.querySelectorAll("a[href]").length > 0'), 'demo index renders links to browser demos');
  await evaluate(`document.querySelectorAll('.demo-card img').forEach(image=>image.loading='eager')`);
  await waitFor(`['grit','snake'].every(id=>{const link=[...document.querySelectorAll('a[href]')].find(a=>new URL(a.href).pathname==='/demos/'+id+'/');const card=link?.closest('.demo-card');const image=card?.querySelector('img');return !!image&&image.complete&&image.naturalWidth>0})`, 'Grit and Snake gallery thumbnails');
  const demoCards = await evaluate(`(() => Object.fromEntries(['grit','snake'].map(id=>{const link=[...document.querySelectorAll('a[href]')].find(a=>new URL(a.href).pathname==='/demos/'+id+'/');const card=link?.closest('.demo-card');const image=card?.querySelector('img');return [id,{loaded:!!image&&image.complete&&image.naturalWidth>0,width:image?.naturalWidth,height:image?.naturalHeight,accessible:!!image&&(image.alt.trim().length>0||image.getAttribute('aria-hidden')==='true'),heading:card?.querySelector('h2')?.textContent.trim(),description:!!card?.querySelector('p')}]})))()`);
  const expectedTitles = { grit: 'Particle Observatory', snake: 'Snake' };
  for (const demo of ['grit', 'snake']) check(demoCards[demo].loaded && demoCards[demo].accessible && demoCards[demo].heading === expectedTitles[demo] && demoCards[demo].description, `${demo} demo gallery thumbnail/card has valid asset and current title (${JSON.stringify(demoCards[demo])})`);
  const galleryOrder = await evaluate(`[...document.querySelectorAll('.demo-card')].map(card=>new URL(card.href).pathname.split('/')[2])`);
  check(galleryOrder.join(',') === 'grit,snake', `demo gallery contains Particle Observatory and Snake (${galleryOrder.join(',')})`);
  check(demoCards.grit.width === 720 && demoCards.grit.height === 420, 'Particle Observatory gallery thumbnail comes from its native 720×420 renderer');
  check(demoCards.snake.width === 960 && demoCards.snake.height === 720, 'Snake gallery thumbnail is the native 960×720 captured frame');
  for (const width of [390, 768, 1440]) {
    const layout = await overflow(width);
    check(layout.document <= layout.viewport + 1, `demo index has no horizontal page overflow at ${width}px`);
  }
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
  await screenshot('demos-index.png');

  await navigate('/demos/snake/');
  check(await evaluate(`document.title.toLowerCase().includes('snake') && !!document.querySelector('iframe[src$="/demos/snake/run.html"]')`), 'Snake page loads the real browser runner');
  const snakeFrame = await attachFrame('/demos/snake/run.html');
  await snakeFrame.waitFor(`document.documentElement?.dataset.runState==='ready'`, 'embedded Snake startup');
  for (const [width, height] of [[1822, 860], [1440, 900], [390, 844], [700, 400], [320, 568]]) {
    await cdp.send('Page.setDeviceMetricsOverride', {width, height, deviceScaleFactor: 1, mobile: false});
    await sleep(500);
    const parent = await evaluate(`(() => {const f=document.querySelector('iframe').getBoundingClientRect();return {scrollW:document.documentElement.scrollWidth,scrollH:document.documentElement.scrollHeight,bottom:f.bottom,title:document.querySelector('h1')?.textContent,source:document.querySelector('.focused-demo-toolbar a')?.href};})()`);
    const childExpression = `(() => {const c=document.querySelector('canvas'),b=c.getBoundingClientRect(),gl=c.getContext('webgl2'),p=new Uint8Array(c.width*c.height*4);gl.bindFramebuffer(gl.READ_FRAMEBUFFER,null);gl.readPixels(0,0,c.width,c.height,gl.RGBA,gl.UNSIGNED_BYTE,p);let green=0,red=0;const colors=new Set();for(let i=0;i<p.length;i+=4){colors.add((p[i]<<16)|(p[i+1]<<8)|p[i+2]);if(p[i+1]>p[i]*1.3&&p[i+1]>p[i+2]*1.3&&p[i+1]>50)green++;if(p[i]>p[i+1]*1.8&&p[i]>p[i+2]*1.8&&p[i]>70)red++;}return {w:innerWidth,h:innerHeight,cw:c.width,ch:c.height,right:b.right,bottom:b.bottom,green,red,colors:colors.size};})()`;
    await snakeFrame.waitFor(`(() => {const s=(${childExpression});return s.w===${width} && s.cw===s.w && s.ch===s.h && s.colors>8;})()`, `Snake resize and rendered frame at ${width}×${height}`, 50, 100);
    const child = await snakeFrame.evaluate(childExpression);
    const settledParent = await evaluate(`(() => {const f=document.querySelector('iframe').getBoundingClientRect();return {scrollW:document.documentElement.scrollWidth,scrollH:document.documentElement.scrollHeight,bottom:f.bottom};})()`);
    check(parent.title==='Snake' && parent.source?.endsWith('/demos/snake/snake.c'), 'Snake has a compact title/source row');
    check(settledParent.scrollW<=width && settledParent.scrollH<=height && settledParent.bottom<=height+1 && child.cw===child.w && child.ch===child.h && child.right<=child.w+1 && child.bottom<=child.h+1, `Snake canvas fits available screen at ${width}×${height}`);
    check(child.colors>8, `Snake remains visibly rendered at ${width}×${height}`);
  }
  await cdp.send('Target.detachFromTarget', {sessionId: snakeFrame.sessionId});
  await navigate('/demos/snake/run.html');
  await waitFor(`document.documentElement.dataset.runState==='ready' && !!document.getElementById('canvas')?.getContext('webgl2')`, 'Snake WebGL2 context');
  await sleep(300);
  check(await evaluate(`document.documentElement.dataset.runState!=='failed' && document.getElementById('canvas').width===innerWidth && document.getElementById('canvas').height===innerHeight`), 'Snake runner starts without runtime errors');
  await screenshot('demo-snake-webgl2.png');

  await navigate('/demos/grit/');
  check(await evaluate(`document.querySelector('h1')?.textContent.trim()==='Particle Observatory' && !!document.querySelector('iframe[src$="/demos/grit/run.html"]')`), 'Particle Observatory page has a compact title and full-screen runner');
  const gritPageFrame = await attachFrame('/demos/grit/run.html');
  await gritPageFrame.waitFor(`(${canvasStateExpression})?.painted > 0`, 'Particle Observatory visible canvas');
  for (const [width, height] of [[1822, 860], [1440, 900], [390, 844], [700, 400], [320, 568]]) {
    await cdp.send('Page.setDeviceMetricsOverride', {width, height, deviceScaleFactor: 1, mobile: false});
    await sleep(250);
    const parent = await evaluate(`(() => {const f=document.querySelector('iframe').getBoundingClientRect();const source=[...document.querySelectorAll('a[href]')].find(a=>a.href.endsWith('/demos/grit/demo.c'));return {scrollW:document.documentElement.scrollWidth,scrollH:document.documentElement.scrollHeight,bottom:f.bottom,title:document.querySelector('h1')?.textContent.trim(),source:source?.href};})()`);
    const childExpression = `(() => {const c=document.querySelector('#canvas'),b=c?.getBoundingClientRect(),d=c?.getContext('2d')?.getImageData(0,0,c.width,c.height).data;let painted=0,colors=new Set();if(d)for(let i=0;i<d.length;i+=4)if(d[i+3]){painted++;colors.add((d[i]<<16)|(d[i+1]<<8)|d[i+2]);}return {w:innerWidth,h:innerHeight,cw:c?.width,ch:c?.height,right:b?.right,bottom:b?.bottom,painted,colors:colors.size};})()`;
    await gritPageFrame.waitFor(`(() => {const s=(${childExpression});return s.w===${width} && s.cw===s.w && s.ch===s.h && s.painted>0 && s.colors>1;})()`, `Particle Observatory resize and rendered frame at ${width}×${height}`, 50, 100);
    const child = await gritPageFrame.evaluate(childExpression);
    const settledParent = await evaluate(`(() => {const f=document.querySelector('iframe').getBoundingClientRect();return {scrollW:document.documentElement.scrollWidth,scrollH:document.documentElement.scrollHeight,bottom:f.bottom};})()`);
    check(parent.title==='Particle Observatory' && parent.source?.endsWith('/demos/grit/demo.c'), 'Particle Observatory has a compact title and source link');
    check(settledParent.scrollW<=width && settledParent.scrollH<=height && settledParent.bottom<=height+1 && child.cw===child.w && child.ch===child.h && child.right<=child.w+1 && child.bottom<=child.h+1, `Particle Observatory canvas fits available screen at ${width}×${height}`);
    check(child.painted>0 && child.colors>1, `Particle Observatory software canvas remains visibly rendered at ${width}×${height}`);
  }
  await cdp.send('Target.detachFromTarget', {sessionId: gritPageFrame.sessionId});
  await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });

  for (const demo of ['grit']) {
    await navigate(`/demos/${demo}/run.html`);
    await waitFor('document.documentElement.dataset.runState === "ready"', 'standalone Particle Observatory runtime');
    await waitFor(`(${canvasStateExpression})?.painted > 0`, 'standalone Particle Observatory first raster');
    const firstFrame = await evaluate(canvasStateExpression);
    await sleep(250);
    const secondFrame = await evaluate(canvasStateExpression);
    check(firstFrame?.painted > 0 && secondFrame?.painted > 0 && firstFrame.hash !== secondFrame.hash, 'standalone Particle Observatory continuously renders changing software raster frames');
    for (const width of [390, 768, 1440]) {
      const layout = await overflow(width);
      check(layout.document <= layout.viewport + 1, 'Particle Observatory standalone runner has no horizontal page overflow at '+width+'px');
      const backing = await evaluate('({w:canvas.width,h:canvas.height,vw:innerWidth,vh:innerHeight})');
      check(backing.w === backing.vw && backing.h === backing.vh, `Particle Observatory standalone canvas backing matches ${width}px viewport (${backing.w}×${backing.h})`);
    }
    await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
    await screenshot(`demo-${demo}-run.png`);
    check(await evaluate('document.documentElement.dataset.runState === "ready" && document.querySelector("#status").hidden'), 'Particle Observatory remains ready without a completion state');
    await captureCanvas('grit');

    await navigate('/demos/');
    const card = await evaluate(`(() => {const element=[...document.querySelectorAll('.demo-card')].find(a=>new URL(a.href).pathname==='/demos/${demo}/');if(!element)return null;const r=element.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
    if (!card) throw new Error(`missing ${demo} gallery card for navigation`);
    await cdp.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: card.x, y: card.y, button: 'left', clickCount: 1 });
    await cdp.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: card.x, y: card.y, button: 'left', clickCount: 1 });
    await waitFor(`location.pathname === "/demos/${demo}/" && document.readyState === "complete"`, `${demo} gallery-card navigation`);
    await waitFor('document.querySelectorAll("iframe").length === 1', `${demo} page to auto-load its iframe`);
    const stage = await evaluate(`(() => {const frame=document.querySelector('iframe');return {sandbox:frame?.getAttribute('sandbox'),src:frame?.getAttribute('src'),title:frame?.title,article:!!document.querySelector('article,.article'),sidebar:!!document.querySelector('.sidebar'),toc:!!document.querySelector('.right-toc,#TableOfContents'),navigation:!!document.querySelector('.prev-next'),controls:!!document.querySelector('[data-demo-start],[data-demo-stop]')};})()`);
    check(stage.sandbox === 'allow-scripts' && !stage.sandbox.includes('allow-same-origin') && stage.src?.endsWith(`/demos/${demo}/run.html`) && stage.title, `${demo} gallery click auto-loads a script-only sandboxed runner`);
    check(!stage.article && !stage.sidebar && !stage.toc && !stage.navigation && !stage.controls, `${demo} run stage omits the article/sidebar/TOC/navigation shell`);
    const embedded = await attachFrame(`/demos/${demo}/run.html`);
    await embedded.waitFor('document.documentElement.dataset.runState === "ready"', 'embedded Particle Observatory runtime');
    await embedded.waitFor(`(${canvasStateExpression})?.painted > 0`, 'embedded Particle Observatory first raster');
    const embeddedFirst = await embedded.evaluate(canvasStateExpression);
    await sleep(250);
    const embeddedSecond = await embedded.evaluate(canvasStateExpression);
    check(embeddedFirst?.painted > 0 && embeddedSecond?.painted > 0 && embeddedFirst.hash !== embeddedSecond.hash, 'embedded Particle Observatory continuously renders changing software raster frames');
    const key = async code => {
      await embedded.evaluate('document.querySelector("#canvas").focus()');
      const value = code==='Space' ? ' ' : code==='KeyR' ? 'r' : 's';
      await cdp.send('Input.dispatchKeyEvent', {type:'keyDown',key:value,code,windowsVirtualKeyCode:code==='Space'?32:code==='KeyR'?82:83}, embedded.sessionId);
      await cdp.send('Input.dispatchKeyEvent', {type:'keyUp',key:value,code,windowsVirtualKeyCode:code==='Space'?32:code==='KeyR'?82:83}, embedded.sessionId);
    };
    await key('Space');
    await sleep(250);
    const pausedFirst = await embedded.evaluate(canvasStateExpression);
    await sleep(200);
    const pausedSecond = await embedded.evaluate(canvasStateExpression);
    check(pausedFirst?.painted > 0 && pausedFirst.hash === pausedSecond?.hash, 'Space pauses Particle Observatory at a stable raster');
    await key('Space');
    await sleep(250);
    const resumed = await embedded.evaluate(canvasStateExpression);
    check(resumed?.painted > 0 && resumed.hash !== pausedSecond?.hash, 'Space resumes Particle Observatory animation');
    await key('KeyR');
    await sleep(100);
    check(await embedded.evaluate('document.documentElement.dataset.runState === "ready" && !!document.querySelector("#canvas").getContext("2d")'), 'R restarts Particle Observatory while its runner stays ready');
    await key('KeyS');
    await sleep(200);
    const stressFrame = await embedded.evaluate(`(() => {const c=document.querySelector('#canvas');const state=(${canvasStateExpression});return {...state,vw:innerWidth,vh:innerHeight};})()`);
    check(stressFrame?.painted > 0 && stressFrame.width===stressFrame.vw && stressFrame.height===stressFrame.vh, 'S switches stress mode with a visible viewport-sized software raster');
    for (const width of [390, 768, 1440]) {
      const layout = await overflow(width);
      check(layout.document <= layout.viewport + 1, `${demo} auto-run stage has no horizontal page overflow at ${width}px`);
    }
    await cdp.send('Page.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
    await screenshot(`demo-${demo}-autostart.png`);
    await cdp.send('Target.detachFromTarget', {sessionId:embedded.sessionId});
  }

  }

  console.log(`Browser checks: ${checks.length}; failures: ${failures.length}; screenshots: ${temp}`);
  for (const item of checks) console.log(`${failures.includes(item) ? 'FAIL' : 'PASS'} ${item}`);
  if (failures.length) process.exitCode = 1;
} catch (error) {
  console.error(`Browser verification could not complete: ${error.stack ?? error.message}`);
  process.exitCode = 2;
} finally {
  cdp?.close();
  browser?.kill('SIGTERM');
  server?.kill('SIGTERM');
  // Preserve screenshots for manual review on success; cleanup on failure is intentionally avoided.
}

