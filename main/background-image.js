/* Shared by the portal and Node tests. The device only receives this bounded
 * format, never a JPEG/PNG decoder or an untrusted full-resolution image. */
(function(root){
  const width=96,height=128,bytes=6184
  function encode(rgba){
    if(!(rgba instanceof Uint8ClampedArray)||rgba.length!==width*height*4)throw Error('图片数据无效')
    const pixels=[]
    for(let n=0;n<rgba.length;n+=4){
      const alpha=rgba[n+3]/255
      pixels.push([0,1,2].map(k=>Math.round(rgba[n+k]*alpha+255*(1-alpha))))
    }
    let boxes=[pixels]
    while(boxes.length<16){
      let selected=-1,spread=-1,channel=0
      boxes.forEach((box,index)=>{
        if(box.length<2)return
        for(let k=0;k<3;k++){
          let lo=255,hi=0;for(const p of box){lo=Math.min(lo,p[k]);hi=Math.max(hi,p[k])}
          const score=(hi-lo)*Math.sqrt(box.length)
          if(score>spread){selected=index;spread=score;channel=k}
        }
      })
      if(selected<0)break
      const box=boxes[selected].slice().sort((a,b)=>a[channel]-b[channel]),half=Math.floor(box.length/2)
      boxes.splice(selected,1,box.slice(0,half),box.slice(half))
    }
    const palette=boxes.map(box=>{
      const sum=box.reduce((a,p)=>a.map((v,k)=>v+p[k]),[0,0,0])
      const [r,g,b]=sum.map(v=>Math.round(v/box.length))
      return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
    })
    while(palette.length<16)palette.push(palette.at(-1)||0)
    const colors=palette.map(v=>[(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31])
    const output=new Uint8Array(bytes);output.set([87,84,66,71,1,width,height,16])
    palette.forEach((v,n)=>{output[8+n*2]=v&255;output[9+n*2]=v>>8})
    pixels.forEach((p,n)=>{
      let best=0,distance=Infinity
      colors.forEach((c,k)=>{const d=(p[0]-c[0])**2+(p[1]-c[1])**2+(p[2]-c[2])**2;if(d<distance){best=k;distance=d}})
      output[40+(n>>1)]|=n%2?best:best<<4
    })
    return output
  }
  function decode(data){
    if(!(data instanceof Uint8Array)||data.length!==bytes||[87,84,66,71,1,width,height,16].some((v,n)=>data[n]!==v))throw Error('背景格式无效')
    const output=new Uint8ClampedArray(width*height*4)
    for(let n=0;n<width*height;n++){
      const index=n%2?data[40+(n>>1)]&15:data[40+(n>>1)]>>4,v=data[8+index*2]|data[9+index*2]<<8
      output.set([(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31,255],n*4)
    }
    return output
  }
  root.WhaleBackground={width,height,bytes,encode,decode}
})(globalThis)
