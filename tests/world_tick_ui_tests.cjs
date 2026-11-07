// Real fixture HTTP JSON -> actual stats.js DOM, not browser/BDS acceptance.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element{
  constructor(){this.value='';this.children=[];this.handlers={};this.textContent='';this.disabled=false;}
  set innerHTML(_){throw Error('Unsafe insertion');}
  append(...v){this.children.push(...v);} replaceChildren(...v){this.children=[...v];}
  addEventListener(name,fn){this.handlers[name]=fn;}
}
const html=fs.readFileSync('companion/web/stats.html','utf8');
const nodes=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>nodes.get('s-'+id);el('category').value='custom';el('key').value='minecraft:total_world_time';el('period').value='all';
let status=200;const calls=[];
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
  document:{getElementById:id=>nodes.get(id),createElement:()=>new Element()},
  Option:function(t,v){this.textContent=t;this.value=v;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async(url,options)=>{calls.push({url,options});return {ok:status===200,status,json:async()=>structuredClone(data)};}
});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  el('key').value='minecraft:total_world_time';el('token').value='fixture-only-token';
  el('login').handlers.submit({preventDefault(){}});await flush();
  assert.equal(el('total').textContent,data.totals.value.toLocaleString('ja-JP')+' tick');
  assert.match(el('status').textContent,/過去のワールド観測値/);
  assert.match(el('scope').textContent,/ワールド共有値/);
  assert.equal(el('ranking').children.length,1);assert.match(el('ranking').children[0].textContent,/順位対象外/);
  assert.equal(el('types').children[0].children[3].disabled,true);
  assert.equal(el('rank-prev').disabled,true);assert.equal(el('rank-next').disabled,true);
  assert.match(el('definition').textContent,/最後のスナップショット/);
  data={...data,status:'recorded_snapshot',totals:{...data.totals,value:0},types:[{...data.types[0],value:0}]};
  el('refresh').handlers.click();await flush();assert.equal(el('total').textContent,'0 tick');
  assert.match(el('status').textContent,/ワールドtickの観測値/);
  data={...data,status:'no_record',totals:{value:null,first_ms:null,last_ms:null},types:[]};
  el('refresh').handlers.click();await flush();assert.match(el('total').textContent,/未計測/);
  status=401;data={error:'期限切れ'};el('refresh').handlers.click();await flush();
  assert.equal(el('ranking').children.length,0);assert.equal(el('total').textContent,'—');
  for(const c of calls){assert.ok(!c.url.includes('fixture-only-token'));assert.equal(c.options.headers.Authorization,'Bearer fixture-only-token');}
  console.log('World tick Japanese DOM PASS: shared/latest/stale/zero/unavailable, no ranking, auth/logout; synthetic only');
})().catch(e=>{console.error(e);process.exitCode=1;});
