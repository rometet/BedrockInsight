// Authenticated JSON -> actual stats.js, not a browser or gameplay attestation.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';this.disabled=false;}
  set innerHTML(_){throw Error('Unsafe insertion');}
  append(...v){this.children.push(...v);} replaceChildren(...v){this.children=[...v];}
  addEventListener(n,fn){this.handlers[n]=fn;}
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
  assert.match(el('title').textContent,/効果の付与観測/);assert.match(el('definition').textContent,/勝利そのものの証明ではありません/);
  assert.equal(el('total').textContent,'3 回');assert.match(el('source').textContent,/ll26.60.raid_effect_changed.v1/);
  assert.ok(el('ranking').children[1].textContent.includes(data.ranking[1].name));
  assert.match(el('status').textContent,/実機受入の証明ではありません/);
  data={...data,status:'no_record',totals:{value:null,first_ms:null,last_ms:null},types:[],ranking:[]};
  el('refresh').handlers.click();await flush();assert.match(el('total').textContent,/未計測/);
  status=401;data={error:'期限切れ'};el('refresh').handlers.click();await flush();
  assert.equal(el('total').textContent,'—');assert.equal(el('ranking').children.length,0);
  for(const c of calls){assert.ok(!c.url.includes('fixture-token'));assert.equal(c.options.headers.Authorization,'Bearer fixture-token');}
  console.log('Raid effect Japanese DOM PASS: effect-only/count/rank/source/null/auth/literal text; synthetic only');
})().catch(e=>{console.error(e);process.exitCode=1;});
