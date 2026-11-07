// Production JS on a synthetic DOM, NOT actual browser/engine acceptance.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';}
  set innerHTML(_){throw Error('unsafe HTML');} append(...v){this.children.push(...v);}
  replaceChildren(...v){this.children=[...v];} addEventListener(n,f){this.handlers[n]=f;}
}
const nodes=new Map([...fs.readFileSync('companion/web/stats.html','utf8').matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>nodes.get('s-'+id);
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
  document:{getElementById:id=>nodes.get(id),createElement:()=>new Element()},
  Option:function(t,v){this.textContent=t;this.value=v;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async()=>({ok:true,status:200,json:async()=>structuredClone(data)})
});
(async()=>{
  el('category').value='mined';el('period').value='all';el('token').value='fixture';
  el('login').handlers.submit({preventDefault(){}});await new Promise(r=>setImmediate(r));
  assert.equal(el('total').textContent,'4 回');
  assert.equal(el('types').children.length,2);assert.equal(el('ranking').children.length,3);
  assert.ok(el('ranking').children[0].textContent.includes('Alice<svg>'));
  assert.ok(el('definition').textContent.includes('旧v1'));assert.ok(el('source').textContent.includes('.v2'));
  assert.ok(el('status').textContent.includes('実機受入の証明ではありません'));
  console.log('Block records Japanese JS PASS: stable filtered totals/rank, v1/v2 scope, names as text; synthetic only');
})().catch(e=>{console.error(e);process.exitCode=1;});
