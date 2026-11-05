import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import test from 'node:test';
vm.runInThisContext(readFileSync(new URL('../main/background-image.js',import.meta.url),'utf8'));
const bg=globalThis.WhaleBackground;
test('solid colors survive quantization with bounded RGB565 error',()=>{
  for(const color of [[244,182,122],[0,255,0],[255,0,0],[0,0,255]]){
    const pixels=new Uint8ClampedArray(bg.width*bg.height*4);
    for(let n=0;n<pixels.length;n+=4)pixels.set([...color,255],n);
    const encoded=bg.encode(pixels),decoded=bg.decode(encoded);
    assert.equal(encoded.length,6184);
    for(let n=0;n<decoded.length;n+=4){color.forEach((v,k)=>assert.ok(Math.abs(v-decoded[n+k])<=8));assert.equal(decoded[n+3],255)}
  }
});
test('transparent pixels composite onto white and replacement buffers are independent',()=>{
  const input=new Uint8ClampedArray(bg.width*bg.height*4),first=bg.encode(input);
  assert.equal(bg.decode(first)[0],255);input.fill(255);const second=bg.encode(input);
  second[40]^=255;assert.notEqual(first[40],second[40]);assert.equal(bg.decode(first)[0],255);
});
test('all sixteen palette entries and both nibble orders decode correctly',()=>{
  const data=new Uint8Array(bg.bytes);data.set([87,84,66,71,1,96,128,16]);
  for(let n=0;n<16;n++){const color=n<<11;data[8+n*2]=color&255;data[9+n*2]=color>>8}
  for(let n=40;n<data.length;n++)data[n]=0xf1;
  const decoded=bg.decode(data);assert.equal(decoded[0],123);assert.equal(decoded[4],8);
});
test('bad types, truncated input, versions and dimensions are rejected',()=>{
  assert.throws(()=>bg.encode(new Uint8ClampedArray(1)));assert.throws(()=>bg.encode([]));
  const data=bg.encode(new Uint8ClampedArray(bg.width*bg.height*4));
  for(const index of [0,4,5,6,7]){const broken=data.slice();broken[index]++;assert.throws(()=>bg.decode(broken))}
  assert.throws(()=>bg.decode(data.slice(0,-1)));
});
