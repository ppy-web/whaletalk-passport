import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import test from 'node:test';

const html=readFileSync(new URL('../main/setup-page.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick=()=>new Promise(resolve=>setImmediate(resolve));
const result=(networks=[])=>({scanned:true,networks:networks.map(ssid=>({ssid,rssi:-50}))});
function page({configured=false,currentFails=false,initial=result(['Home','Hotspot'])}={}) {
  const elements=new Map(),calls=[];
  for(const [,id] of html.matchAll(/\bid="([^"]+)"/g))elements.set(id,{
    value:'',textContent:'',hidden:false,disabled:false,required:false,checked:false,options:[],
    replaceChildren(...options){this.options=options;this.value=options[0]?.value||''},
    add(option){this.options.push(option)}
  });
  const el=id=>elements.get(id);
  el('wifi').value='';el('port').value='3101';
  el('f').elements=Object.fromEntries(['password','ip','token','port'].map(id=>[id,el(id)]));
  let scan=async()=>result(['Hotspot','Home']),save=async()=>({ok:true,text:async()=> 'saved'});
  const context=vm.createContext({
    document:{getElementById:el},
    Option:function(text,value){return {text,value}},AbortSignal,
    FormData:class { constructor(){this.items=Object.entries(el('f').elements).filter(([,field])=>!field.disabled).map(([name,field])=>[name,field.value])} [Symbol.iterator](){return this.items[Symbol.iterator]()} },
    fetch:async(url,options={})=>{
      calls.push({url,options});
      if(url==='/save')return save();
      if(url==='/current'&&currentFails)throw Error('offline');
      const data=url==='/current'?{nonce:'test-nonce',configured,ssid:'Saved',ip:'192.168.1.2',port:3101}:url==='/scan'?await scan():initial;
      return {ok:true,json:async()=>data};
    }
  });
  vm.runInContext(script,context);
  return {el,calls,setScan:value=>scan=value,setSave:value=>save=value,submit:()=>el('f').onsubmit({preventDefault(){}})};
}
test('refresh requests a new authenticated scan, keeps selection by SSID and form values',async()=>{
  const p=page();await tick();p.el('wifi').value='0';p.el('password').value='private';p.el('ip').value='192.168.1.5';p.el('token').value='0012';
  let complete;p.setScan(()=>new Promise(resolve=>complete=resolve));
  const pending=p.el('refresh').onclick();
  assert.equal(p.el('refresh').disabled,true);assert.equal(p.el('save').disabled,true);
  const request=p.calls.at(-1);assert.equal(request.url,'/scan');assert.equal(request.options.method,'POST');assert.equal(request.options.headers['X-Setup-Nonce'],'test-nonce');
  await tick();complete(result(['Hotspot','Home']));await pending;
  assert.equal(p.el('wifi').value,'1');assert.equal(p.el('password').value,'private');assert.equal(p.el('ip').value,'192.168.1.5');assert.equal(p.el('token').value,'0012');assert.equal(p.el('refresh').disabled,false);
});
test('edits to a hidden network during scanning survive',async()=>{
  const p=page();await tick();let complete;p.setScan(()=>new Promise(resolve=>complete=resolve));
  const pending=p.el('refresh').onclick();await tick();p.el('wifi').value='manual';p.el('manual').value='Hidden';p.el('wifi').onchange();complete(result(['Home']));await pending;
  assert.equal(p.el('wifi').value,'manual');assert.equal(p.el('manual').value,'Hidden');assert.equal(p.el('manual').disabled,false);
});
test('a disappearing network becomes manual input and empty scans remain usable',async()=>{
  const p=page();await tick();p.el('wifi').value='0';p.setScan(async()=>result());await p.el('refresh').onclick();
  assert.equal(p.el('wifi').value,'manual');assert.equal(p.el('manual').value,'Home');assert.equal(p.el('save').disabled,false);
});
test('scan errors and cancelled requests retain old choices and permit retry',async()=>{
  for(const error of [new Error('timeout'),Object.assign(new Error('cancelled'),{name:'AbortError'})]){
    const p=page();await tick();p.el('wifi').value='1';p.setScan(async()=>{throw error});await p.el('refresh').onclick();
    assert.equal(p.el('wifi').value,'1');assert.equal(p.el('wifi').options.length,4);assert.ok(p.el('scanHint').textContent);assert.equal(p.el('refresh').disabled,false);
    p.setScan(async()=>result(['Hotspot']));await p.el('refresh').onclick();assert.equal(p.el('wifi').value,'0');
  }
});
test('failed scans do not replace valid results, and no nonce prevents scanning/saving',async()=>{
  const p=page();await tick();p.el('wifi').value='0';p.setScan(async()=>({scanned:false,networks:[]}));await p.el('refresh').onclick();assert.equal(p.el('wifi').value,'0');
  const offline=page({currentFails:true});await tick();assert.equal(offline.el('refresh').disabled,true);assert.equal(offline.el('save').disabled,true);
  await offline.el('refresh').onclick();await offline.submit();assert.equal(offline.calls.filter(c=>c.url==='/scan'||c.url==='/save').length,0);
});
test('keep-Wi-Fi, pairing-code zeros, save retry and successful save retain their behavior',async()=>{
  const p=page({configured:true});await tick();assert.equal(p.el('wifi').disabled,true);assert.equal(p.el('password').disabled,true);
  p.el('token').value='0012';p.setSave(async()=>({ok:false,text:async()=> 'retry'}));await p.submit();
  const data=JSON.parse(p.calls.at(-1).options.body);assert.equal(data.keepWifi,true);assert.equal(data.token,'0012');assert.equal(data.port,3101);assert.equal('password' in data,false);
  assert.equal(p.el('save').disabled,false);p.setSave(async()=>({ok:true,text:async()=> 'saved'}));await p.submit();assert.equal(p.el('save').disabled,true);assert.equal(p.el('refresh').disabled,true);
});
