// Deterministic DOM-level UI tests, not a browser/screenshot or gameplay claim.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
class Element {
  constructor(tag='div') { this.tag=tag; this.value='';this.textContent='';this.children=[];this.handlers={}; }
  set innerHTML(_) { throw new Error('Unsafe HTML insertion'); }
  append(...items) { this.children.push(...items); }
  replaceChildren(...items) { this.children=[...items]; }
  addEventListener(name,handler) { this.handlers[name]=handler; }
}
const html=fs.readFileSync('companion/web/index.html','utf8');
const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>{assert.ok(elements.has(id),'Missing actual HTML element '+id);return elements.get(id);};
el('period').value='day';el('metric').value='playtime';
const name='<img src=x onerror=alert(1)>';
const player={key:'a'.repeat(32),name,playtime_text:'0時間 2分 16秒',sessions:1,destroy_attempts:2,place_attempts:1,joins:1,leaves:1,open_sessions:1,invalid_sessions:0,last_active_ms:1793911705516};
const report={period:{label:'2026-11-06',timezone:'JST (UTC+09:00)'},totals:{playtime_text:player.playtime_text,sessions:1,destroy_attempts:2,place_attempts:1,open_sessions:1},players:[player],selected_player:player,
  ranking:[{rank:1,name,display:player.playtime_text}],unmeasured:['死亡','クラフト'],server:{effective_tps:null,processing_mspt:null,current_online:null,peak_closed_lower_bound:1,reason:'tick sampler 未接続',sample_count:0,last_sample_ms:null},generated_at_ms:1793911705516,last_event_ms:1793911705516,semantics:'完結のみ・試行は成功ではありません'};
const calls=[];
vm.runInNewContext(fs.readFileSync('companion/web/app.js','utf8'),{
  document:{getElementById:el,createElement:tag=>new Element(tag)},
  Option:function(text,value){this.textContent=text;this.value=value;},
  URLSearchParams,Intl,Date,setInterval:()=>0,
  fetch:async(url,options)=>{calls.push({url,options});return {ok:true,json:async()=>report};}
});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  assert.equal(calls.length,0,'No unauthenticated data fetch');
  el('token').value='test-fixture-token-not-a-real-credential';
  el('login').handlers.submit({preventDefault(){}});await flush();
  assert.equal(el('token').value,'');assert.equal(calls[0].options.credentials,'omit');
  assert.equal(calls[0].options.headers.Authorization,'Bearer test-fixture-token-not-a-real-credential');
  assert.equal(el('total-time').textContent,'0時間 2分 16秒');
  assert.equal(el('total-attempts').textContent,'2 / 1');
  assert.equal(el('tps').textContent,'未計測');assert.equal(el('mspt').textContent,'未計測');assert.equal(el('online').textContent,'未計測');
  assert.equal(el('rankings').children[0].children[0].textContent,'1. '+name);
  assert.equal(el('player').children[1].textContent,name);
  assert.ok(el('unsupported').textContent.includes('未計測: 死亡'));
  el('period').value='month';el('period').handlers.change();await flush();assert.ok(calls.at(-1).url.includes('period=month'));
  assert.match(el('date').value,/^\d{4}-\d{2}$/);
  el('period').value='all';el('period').handlers.change();await flush();assert.ok(!new URLSearchParams(calls.at(-1).url.split('?')[1]).has('date'));
  el('metric').value='place_attempts';el('metric').handlers.change();await flush();assert.ok(calls.at(-1).url.includes('metric=place_attempts'));
  el('player').value=player.key;el('player').handlers.change();await flush();assert.ok(calls.at(-1).url.includes('player='+player.key));
  assert.equal(el('profile').children[1].textContent,name);
  report.ranking=[];report.server.peak_closed_lower_bound=null;el('refresh').handlers.click();await flush();
  assert.ok(el('rankings').children[0].textContent.includes('正の記録はありません'));assert.equal(el('peak').textContent,'未集計');
  report.server={...report.server,effective_tps:18.75,current_online:2,sample_count:100,last_sample_ms:1793911705516,reason:'同一Level通知の実測。処理時間MSPTではありません。'};
  el('refresh').handlers.click();await flush();
  assert.equal(el('tps').textContent,'18.75');assert.equal(el('online').textContent,'2');assert.equal(el('mspt').textContent,'未計測');
  assert.ok(el('sampling').textContent.includes('実測'));
  report.server={...report.server,processing_mspt:2,reason:'Level::$tick処理中の実測時間÷実行回数。最内周1tick最大2ms。'};
  el('refresh').handlers.click();await flush();
  assert.equal(el('mspt').textContent,'2');assert.ok(el('sampling').textContent.includes('Level::$tick'));
  report.server={...report.server,effective_tps:null,current_online:null,processing_mspt:null,reason:'最終実測から20秒超。現在値は非表示。'};
  el('refresh').handlers.click();await flush();
  assert.equal(el('tps').textContent,'未計測');assert.equal(el('online').textContent,'未計測');
  assert.equal(el('mspt').textContent,'未計測');
  assert.ok(el('sampling').textContent.includes('20秒超'));
  el('logout').handlers.click();const count=calls.length;el('refresh').handlers.click();await flush();
  assert.equal(calls.length,count);assert.equal(el('player').children.length,1);assert.equal(el('rankings').children.length,0);
  assert.equal(el('total-time').textContent,'—');
  console.log('Dashboard DOM PASS: auth flow, Japanese 4-category rendering, literal names, selectors, empty/stale samples, logout; not a browser rendering proof');
})().catch(error=>{console.error(error);process.exitCode=1;});
