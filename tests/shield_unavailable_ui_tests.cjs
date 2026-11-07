// Existing stats.js must keep shield unavailable even when raw evidence exists.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
 constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';this.disabled=false;}
 set innerHTML(_){throw Error('Unsafe insertion');} append(...v){this.children.push(...v);}
 replaceChildren(...v){this.children=[...v];} addEventListener(n,fn){this.handlers[n]=fn;}
}
const nodes=new Map([...fs.readFileSync('companion/web/stats.html','utf8').matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>nodes.get('s-'+id);el('category').value='custom';el('key').value=data.key;el('period').value='all';
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
 document:{getElementById:id=>nodes.get(id),createElement:()=>new Element()},
 Option:function(t,v){this.textContent=t;this.value=v;},Intl,Date,URLSearchParams,setInterval:()=>0,
 fetch:async()=>({ok:true,status:200,json:async()=>structuredClone(data)})
});
(async()=>{
 el('key').value=data.key;el('token').value='fixture-token';el('login').handlers.submit({preventDefault(){}});
 await new Promise(resolve=>setImmediate(resolve));
 assert.match(el('title').textContent,/盾/);assert.match(el('status').textContent,/未対応.*未計測/);
 assert.match(el('total').textContent,/未計測/);assert.equal(data.types.length,0);
 assert.match(el('types').children[0].textContent,/未対応・未計測を0にしません/);
 assert.equal(data.ranking.length,0);assert.equal(data.projection_supported,false);
 console.log('Shield remains unavailable in actual Japanese JS; raw flags are NOT statistics');
})().catch(e=>{console.error(e);process.exitCode=1;});
