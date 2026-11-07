// HTTP payload -> real stats.js in synthetic DOM, not gameplay/browser evidence.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';this.disabled=false;}
  set innerHTML(_){throw Error('Unsafe insertion');} append(...v){this.children.push(...v);}
  replaceChildren(...v){this.children=[...v];} addEventListener(n,fn){this.handlers[n]=fn;}
}
const html=fs.readFileSync('companion/web/stats.html','utf8');
const nodes=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>nodes.get('s-'+id);el('category').value='custom';el('key').value=data.key;el('period').value='all';
let status=200;const calls=[];
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
  document:{getElementById:id=>nodes.get(id),createElement:()=>new Element()},
  Option:function(t,v){this.textContent=t;this.value=v;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async(url,options)=>{calls.push({url,options});return {ok:status===200,status,json:async()=>structuredClone(data)};}
});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  el('key').value=data.key;el('token').value='fixture-token';el('login').handlers.submit({preventDefault(){}});await flush();
  assert.match(el('title').textContent,/睡眠観測後.*最終保存値/);assert.match(el('definition').textContent,/50ms壁時計換算/);
  assert.equal(el('total').textContent,'200 tick');assert.match(el('status').textContent,/現在値・実機受入の証明ではありません/);
  assert.ok(el('ranking').children[0].textContent.includes(data.ranking[0].name));
  data={...data,totals:{value:0,source_rows:1,first_ms:1,last_ms:1},types:[],ranking:[]};
  el('refresh').handlers.click();await flush();assert.equal(el('total').textContent,'0 tick');
  data={...data,status:'no_record',totals:{value:null,source_rows:0,first_ms:null,last_ms:null}};
  el('refresh').handlers.click();await flush();assert.match(el('total').textContent,/未計測/);
  status=401;data={error:'期限切れ'};el('refresh').handlers.click();await flush();assert.equal(el('total').textContent,'—');
  for(const c of calls){assert.ok(!c.url.includes('fixture-token'));assert.equal(c.options.headers.Authorization,'Bearer fixture-token');}
  console.log('Rest Japanese DOM PASS: saved snapshot/units/rank/known zero/unknown/auth; synthetic only');
})().catch(e=>{console.error(e);process.exitCode=1;});
