import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import test from 'node:test';
const html=readFileSync(new URL('../main/appearance-page.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick=()=>new Promise(resolve=>setImmediate(resolve));
function page({offline=false,background=0}={}){
  const elements=new Map(),calls=[],revoked=[];
  for(const [,id] of html.matchAll(/\bid="([^"]+)"/g))elements.set(id,{disabled:['upload','apply','file'].includes(id),style:{},files:[],textContent:''});
  const radios=[0,1,2,3].map(n=>({value:String(n),checked:n===0,disabled:n===2}));
  let upload=async()=>({ok:true,text:async()=> 'uploaded'}),apply=async()=>({ok:true,text:async()=> 'saved'}),decode=async()=>{};
  const el=id=>elements.get(id);
  const context=vm.createContext({
    Uint8Array,Uint8ClampedArray,AbortSignal,
    URL:{createObjectURL:()=> 'blob:'+Math.random(),revokeObjectURL:url=>revoked.push(url)},
    Image:class {naturalWidth=240;naturalHeight=320;decode(){return decode()}},
    ImageData:class {constructor(data,width,height){Object.assign(this,{data,width,height})}},
    document:{getElementById:el,querySelectorAll:()=>radios,createElement:()=>({
      getContext:()=>({fillRect(){},drawImage(){},getImageData:()=>({data:new Uint8ClampedArray(96*128*4)}),putImageData(){}}),
      toDataURL:()=> 'data:image/png;base64,preview'
    })},
    fetch:async(url,options={})=>{
      calls.push({url,options});if(url==='/background/upload')return upload();if(url==='/background/apply')return apply();
      if(offline)throw Error('offline');return {ok:true,json:async()=>({nonce:'test-nonce',background,customBackground:false})};
    }
  });
  vm.runInContext(readFileSync(new URL('../main/background-image.js',import.meta.url),'utf8'),context);
  vm.runInContext(script,context);
  return {el,radios,calls,revoked,setUpload:v=>upload=v,setApply:v=>apply=v,setDecode:v=>decode=v,
    choose:async(type='image/png',size=1024)=>{el('file').files=[{type,size}];await el('file').onchange()}};
}
test('no nonce keeps writes disabled and handlers do not send requests',async()=>{
  const p=page({offline:true});await tick();assert.equal(p.el('apply').disabled,true);assert.equal(p.el('upload').disabled,true);
  await p.el('apply').onclick();await p.el('upload').onclick();assert.equal(p.calls.length,1);
});
test('prepared image must upload, failed uploads retry, save failure retries without credentials',async()=>{
  const p=page();await tick();await p.choose();assert.equal(p.el('apply').disabled,true);assert.equal(p.el('upload').disabled,false);
  await p.el('apply').onclick();assert.equal(p.calls.filter(c=>c.url==='/background/apply').length,0);
  p.setUpload(async()=>({ok:false,text:async()=> 'full'}));await p.el('upload').onclick();assert.equal(p.el('upload').disabled,false);assert.equal(p.radios[2].disabled,true);
  p.setUpload(async()=>({ok:true,text:async()=> 'uploaded'}));await p.el('upload').onclick();
  const request=p.calls.at(-1);assert.equal(request.options.body.length,6184);assert.equal(request.options.headers['X-Setup-Nonce'],'test-nonce');
  assert.equal(p.radios[2].checked,true);assert.equal(p.el('apply').disabled,false);assert.equal(p.el('upload').disabled,true);
  p.setApply(async()=>({ok:false,text:async()=> 'retry'}));await p.el('apply').onclick();assert.equal(p.el('apply').disabled,false);
  const body=JSON.parse(p.calls.at(-1).options.body);assert.deepEqual(body,{background:2});
  p.setApply(async()=>({ok:true,text:async()=> 'saved'}));await p.el('apply').onclick();assert.equal(p.el('apply').disabled,true);
});
test('switching back to a built-in discards an unuploaded draft',async()=>{
  const p=page();await tick();await p.choose();p.radios.forEach(r=>r.checked=r.value==='1');p.radios[1].onchange();
  assert.equal(p.el('apply').disabled,false);assert.equal(p.el('upload').disabled,true);assert.equal(p.el('preview').src,'/background-night.png');
  await p.el('apply').onclick();assert.deepEqual(JSON.parse(p.calls.at(-1).options.body),{background:1});
});
test('image decode races and file cancellation release URLs and preserve latest selection',async()=>{
  const p=page();await tick();let first;p.setDecode(()=>new Promise(resolve=>first=resolve));
  const early=p.choose();await tick();p.radios.forEach(r=>r.checked=r.value==='1');p.radios[1].onchange();first();await early;
  assert.equal(p.el('preview').src,'/background-night.png');assert.equal(p.el('upload').disabled,true);assert.equal(p.revoked.length,1);
  p.setDecode(async()=>{});await p.choose();p.el('file').files=[];await p.el('file').onchange();assert.equal(p.el('upload').disabled,true);assert.equal(p.el('apply').disabled,false);
});
test('oversized files, unsupported formats and failed decoding cannot upload',async()=>{
  const p=page();await tick();for(const [type,size] of [['image/png',9*1024*1024],['image/svg+xml',100]]){await p.choose(type,size);assert.equal(p.el('upload').disabled,true)}
  p.setDecode(async()=>{throw Error('decode failed')});await p.choose();assert.equal(p.el('upload').disabled,true);assert.equal(p.revoked.length,1);
});

test('off preference loads, previews and saves without discarding custom data',async()=>{
  const p=page({background:3});await tick();
  assert.equal(p.radios[3].checked,true);assert.equal(p.el('preview').style.visibility,'hidden');
  assert.match(p.el('previewHint').textContent,/关闭背景/);
  await p.el('apply').onclick();assert.deepEqual(JSON.parse(p.calls.at(-1).options.body),{background:3});
});
