// Actual authenticated fixture API JSON on stdin; not BDS/gameplay acceptance.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const original=JSON.parse(fs.readFileSync(0,'utf8'));
let data=structuredClone(original),status=200,hold=false;
class Element {
  constructor(){this.value='';this.textContent='';this.children=[];this.handlers={};this.disabled=false;}
  set innerHTML(_){throw Error('Unsafe HTML insertion');}
  append(...values){this.children.push(...values);}
  replaceChildren(...values){this.children=[...values];}
  addEventListener(name,fn){this.handlers[name]=fn;}
}
const html=fs.readFileSync('companion/web/stats.html','utf8');
const categoryOptions=[...html.split('id="s-category"')[1].split('</select>')[0].matchAll(/value="([^"]+)"/g)].map(m=>'minecraft:'+m[1]);
assert.deepEqual(categoryOptions,original.catalog.categories.map(c=>c.id));
assert.ok(html.includes('href="/"'));assert.ok(html.includes('href="/actions"'));
const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>{assert.ok(elements.has('s-'+id),id);return elements.get('s-'+id);};
el('period').value='all';el('category').value='dropped';el('key').value='minecraft:play_time';
const calls=[],pending=[];let interval;
vm.runInNewContext(fs.readFileSync('companion/web/stats.js','utf8'),{
  document:{getElementById:id=>elements.get(id),createElement:()=>new Element()},
  Option:function(text,value){this.textContent=text;this.value=value;},Intl,Date,URLSearchParams,
  setInterval:(fn,ms)=>{assert.equal(ms,30000);interval=fn;return 0;},
  fetch:(url,options)=>{
    calls.push({url,options});
    const response={ok:status===200,status,json:async()=>structuredClone(data)};
    return hold?new Promise(resolve=>pending.push(()=>resolve(response))):Promise.resolve(response);
  },
});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
const query=()=>new URL(calls.at(-1).url,'http://127.0.0.1').searchParams;
const change=async(id,value)=>{el(id).value=value;el(id).handlers.change();await flush();};
const click=async(id)=>{el(id).handlers.click();await flush();};
const login=async()=>{el('token').value='fixture-only-not-a-real-secret';el('login').handlers.submit({preventDefault(){}});await flush();};
function emptyResults(){assert.equal(el('types').children.length,0);assert.equal(el('ranking').children.length,0);assert.equal(el('total').textContent,'—');}
(async()=>{
  assert.equal(calls.length,0);interval();assert.equal(calls.length,0);
  await login();assert.equal(el('token').value,'');
  assert.equal(el('key').children.length,77);assert.equal(el('key').disabled,true);
  assert.equal(el('total').textContent,'3 個');
  assert.equal(el('types').children[0].children[0].textContent,'石');
  assert.equal(el('types').children[0].children[1].textContent,'minecraft:stone');
  assert.equal(el('types').children[0].children[2].textContent,'3 個');
  assert.equal(el('player').children[1].textContent,'<svg onload=alert(1)>');
  assert.ok(el('ranking').children[0].textContent.includes('<svg onload=alert(1)>'));
  assert.ok(el('status').textContent.includes('完全収集・実機受入の証明ではありません'));
  assert.ok(el('updated').textContent.includes('DLL実機受入: 未証明'));
  assert.ok(el('period-label').textContent.includes('JST'));
  assert.equal(el('prev').disabled,true);assert.equal(el('next').disabled,true);
  data.totals.value=Number.MAX_SAFE_INTEGER+1;await click('refresh');
  assert.equal(el('total').textContent,'表示精度の範囲外（API原文を確認）');
  data=structuredClone(original);await click('refresh');
  const key=original.players[0].key;
  await change('player',key);assert.equal(query().get('player'),key);
  el('search').value="石%' OR 1=1 --";await click('filter');
  assert.equal(query().get('search'),"石%' OR 1=1 --");
  el('types').children[0].children[3].handlers.click();await flush();
  assert.equal(query().get('type'),'minecraft:stone');assert.equal(query().get('rank_page'),'0');
  await click('reset');assert.equal(query().has('type'),false);assert.equal(query().has('search'),false);
  await change('period','month');assert.match(el('date').value,/^\d{4}-\d{2}$/);
  assert.equal(query().get('period'),'month');assert.equal(query().get('date'),el('date').value);
  await change('period','day');assert.match(el('date').value,/^\d{4}-\d{2}-\d{2}$/);
  await change('period','all');assert.equal(query().has('date'),false);
  data.has_more=data.ranking_has_more=data.players_has_more=true;
  await click('refresh');assert.equal(el('next').disabled,false);
  await click('next');assert.equal(query().get('page'),'1');assert.equal(query().get('rank_page'),'0');
  await click('rank-next');assert.equal(query().get('page'),'1');assert.equal(query().get('rank_page'),'1');
  await click('players-next');assert.equal(query().get('players_page'),'1');
  await click('prev');await click('rank-prev');await click('players-prev');
  assert.equal(query().get('page'),'0');assert.equal(query().get('rank_page'),'0');assert.equal(query().get('players_page'),'0');
  await change('category','custom');assert.equal(query().get('key'),'minecraft:play_time');assert.equal(el('key').disabled,false);
  data={...data,key:'minecraft:damage_taken',label:'受けたダメージ',unit:'1/10単位',status:'unsupported',
    totals:{value:null,first_ms:null,last_ms:null},sources:[],types:[],ranking:[],
    definition:'確定結果の読取経路は未対応。',has_more:false,ranking_has_more:false,players_has_more:false};
  await change('key','minecraft:damage_taken');assert.equal(query().get('key'),'minecraft:damage_taken');
  assert.equal(el('total').textContent,'未計測／記録なし');
  assert.ok(el('status').textContent.includes('未対応／未計測'));
  assert.ok(el('types').children[0].textContent.includes('未計測を0にしません'));
  assert.ok(el('ranking').children[0].textContent.includes('未計測を0にしません'));
  assert.ok(el('source').textContent.includes('読取経路なし'));
  data=structuredClone(original);data.selected_player={key,name:'<svg onload=alert(1)>'};data.players=[];
  await click('refresh');assert.equal(el('player').children[1].textContent,data.selected_player.name);
  // Requests that finish after a newer request or logout cannot repopulate data.
  hold=true;el('refresh').handlers.click();emptyResults();assert.equal(pending.length,1);
  hold=false;data={...original,label:'新しい結果'};await click('refresh');
  assert.ok(el('title').textContent.includes('新しい結果'));pending.shift()();await flush();
  assert.ok(el('title').textContent.includes('新しい結果'));
  status=503;data={error:'読み取り待機'};await click('refresh');emptyResults();
  assert.equal(el('status').textContent,'取得失敗');assert.equal(el('error').textContent,'読み取り待機');
  status=200;data=structuredClone(original);await click('refresh');
  hold=true;el('refresh').handlers.click();el('logout').handlers.click();emptyResults();
  pending.shift()();hold=false;await flush();emptyResults();
  assert.equal(el('player').children.length,1);assert.equal(el('key').children.length,1);
  const n=calls.length;interval();await click('refresh');assert.equal(calls.length,n);
  await login();status=401;data={error:'トークン期限切れ'};await click('refresh');emptyResults();
  assert.equal(el('status').textContent,'再認証が必要');assert.equal(el('player').children.length,1);
  assert.equal(el('token').value,'');const after=calls.length;interval();await click('refresh');assert.equal(calls.length,after);
  for(const {url,options} of calls){
    assert.ok(!url.includes('secret'));assert.ok(!url.includes('private-uuid'));
    assert.equal(options.credentials,'omit');assert.equal(options.cache,'no-store');
    assert.equal(options.headers.Authorization,'Bearer fixture-only-not-a-real-secret');
  }
  console.log('Stats library DOM PASS: authenticated fixture JSON, 9/77 selectors, literal names, units, paging, filters, unsupported state, races, logout/401; not gameplay');
})().catch(error=>{console.error(error);process.exitCode=1;});
