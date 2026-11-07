// Shared label file -> actual stats.js with synthetic DOM; not browser proof.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';this.disabled=false;}
  set innerHTML(_){throw Error('Label inserted as HTML');} append(...v){this.children.push(...v);}
  replaceChildren(...v){this.children=[...v];} addEventListener(n,fn){this.handlers[n]=fn;}
}
const nodes=new Map([...fs.readFileSync('companion/web/stats.html','utf8').matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>nodes.get('s-'+id);el('category').value='mined';el('period').value='all';
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
  document:{getElementById:id=>nodes.get(id),createElement:()=>new Element()},
  Option:function(t,v){this.textContent=t;this.value=v;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async()=>({ok:true,status:200,json:async()=>structuredClone(data)})
});
(async()=>{
  el('category').value='mined';el('token').value='fixture-token';el('login').handlers.submit({preventDefault(){}});
  await new Promise(resolve=>setImmediate(resolve));
  const seen=el('types').children.map(row=>({label:row.children[0].textContent,id:row.children[1].textContent}));
  assert.ok(seen.some(row=>row.label==='星の石' && row.id==='minecraft:stone'));
  assert.ok(seen.some(row=>row.label==='追加<部品>" &' && row.id==='addon:widget'));
  assert.equal(el('total').textContent,'6 回');
  console.log('Type-label Japanese JS PASS: configurable labels as text, stable canonical IDs and values; synthetic only');
})().catch(error=>{console.error(error);process.exitCode=1;});
