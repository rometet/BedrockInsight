// JSON on stdin comes from the authenticated fixture HTTP API. Not gameplay.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.textContent='';this.children=[];this.handlers={};}
  set innerHTML(_){throw Error('Unsafe HTML insertion');}
  append(...v){this.children.push(...v);} replaceChildren(...v){this.children=[...v];}
  addEventListener(name,fn){this.handlers[name]=fn;}
}
const html=fs.readFileSync('companion/web/actions.html','utf8');
const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>{assert.ok(elements.has('a-'+id),id);return elements.get('a-'+id);};
el('period').value='all';el('metric').value='dropped_items';
const calls=[];
vm.runInNewContext(fs.readFileSync('companion/web/actions.js','utf8'),{
  document:{getElementById:id=>elements.get(id),createElement:()=>new Element()},
  Option:function(text,value){this.textContent=text;this.value=value;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async(url,options)=>{calls.push({url,options});return {ok:true,json:async()=>data};}
});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  assert.equal(calls.length,0);el('token').value='fixture-only-not-real-credential';
  el('login').handlers.submit({preventDefault(){}});await flush();
  assert.equal(el('token').value,'');assert.equal(calls[0].options.credentials,'omit');
  assert.ok(!calls[0].url.includes('credential'));assert.ok(calls[0].options.headers.Authorization);
  assert.equal(el('player').children[1].textContent,data.players[0].name);
  assert.ok(el('rankings').children[0].textContent.includes(data.ranking[0].name));
  assert.ok(el('profile').children.some(c=>c.textContent==='64 個'));
  assert.ok(el('types').children[0].textContent.includes('minecraft:stone — 64 個 / 1 回'));
  assert.ok(el('definition').textContent.includes('インベントリ差分'));
  assert.ok(el('profile').children.some(c=>c.textContent==='死亡フラグ成立（処理後）'));
  const confirmed=data.selected_player ? data.selected_player.confirmed_deaths : data.totals.confirmed_deaths;
  assert.ok(el('profile').children.some(c=>c.textContent===(confirmed == null ? '未計測／記録なし' : confirmed+' 回')));
  for (const cause of data.player_deaths.causes) {
    assert.ok(el('deaths').children.some(c=>c.textContent===cause.label+' ('+cause.code+') — '+cause.count+' 回'));
  }
  assert.ok(el('death-definition').textContent.includes('キラー帰属'));
  for(const t of (data.killed_by?.types || [])) {
    assert.ok(el('killed-by').children.some(c=>c.textContent===t.type+' / '+(t.path==='direct'?'直接攻撃':'子飛び道具のsource所有者')+' — '+t.count+' 回'));
  }
  assert.ok(el('killed-by-definition').textContent.includes('原因コード'));
  for(const t of (data.projectile_kills?.types || [])) {
    assert.ok(el('projectile-kills').children.some(c=>c.textContent===t.type+' / '+(t.kind==='player'?'プレイヤー':'Mob')+' — '+t.count+' 回'));
  }
  assert.ok(el('profile').children.some(c=>c.textContent==='スプリント開始／終了フラグ観測'));
  for(const mode of data.player_modes.modes) {
    const n=x=>x==null?'未計測／記録なし':x+' 回';
    assert.ok(el('modes').children.some(c=>c.textContent===(mode.mode==='sprint'?'スプリント':'スニーク')+' — 開始 '+n(mode.starts)+' / 終了 '+n(mode.stops)));
  }
  assert.ok(el('mode-definition').textContent.includes('継続時間'));
  assert.ok(el('profile').children.some(c=>c.textContent==='拾得処理中・主所持品の増加'));
  for(const type of data.item_pickups.types) {
    assert.ok(el('pickups').children.some(c=>c.textContent===type.type+' — '+type.items+' 個 / '+type.actions+' 回'));
  }
  assert.ok(el('pickup-definition').textContent.includes('前後差分'));
  assert.equal(el('jump-count').textContent,data.player_jumps.count==null?'未計測／記録なし':data.player_jumps.count+' 回');
  assert.ok(el('jump-definition').textContent.includes('空中移動'));
  for(const victim of data.direct_kills.types) {
    assert.ok(el('kills').children.some(c=>c.textContent===victim.type+' / '+(victim.kind==='player'?'プレイヤー':'Mob')+' — '+victim.count+' 回'));
  }
  assert.ok(el('kill-definition').textContent.includes('飛び道具'));
  for(const type of data.item_use_results.types) {
    const n=type.items==null?'未計測／記録なし':type.items+' 個';
    assert.ok(el('use-results').children.some(c=>c.textContent===type.type+' / '+(type.hand==='main_hand'?'主手':'副手')+' — true応答 '+type.actions+' 回 / 主所持品減少 '+n));
  }
  assert.ok(el('use-definition').textContent.includes('消費成功ではありません'));
  assert.ok(el('profile').children.some(c=>c.textContent==='ブロック採掘の成功（種類別）'));
  for(const type of data.blocks_mined.types) {
    assert.ok(el('mined').children.some(c=>c.textContent===type.type+' — '+type.count+' 回'));
  }
  assert.ok(el('mined-definition').textContent.includes('全破壊数'));
  for(const type of data.blocks_placed.types) {
    assert.ok(el('placed').children.some(c=>c.textContent===type.type+' — '+type.count+' 回'));
  }
  assert.ok(el('placed-definition').textContent.includes('全設置数'));
  assert.ok(el('profile').children.some(c=>c.textContent==='クラフト成立の回数'));
  for(const type of data.items_crafted.types) {
    assert.ok(el('crafted').children.some(c=>c.textContent===type.type+' — '+type.items+' 個 / '+type.actions+' 回'));
  }
  assert.ok(el('crafted-definition').textContent.includes('最終コミットは未検証'));
  for(const mode of data.travel_distance.modes) {
    assert.ok(el('travel').children.some(c=>c.textContent===mode.mode+' — '+mode.cm+' cm'));
  }
  assert.ok(el('travel-definition').textContent.includes('全移動距離'));
  for(const item of data.block_interactions.types) {
    assert.ok(el('interactions').children.some(c=>c.textContent===item.key+' — '+item.count+' 回'));
  }
  assert.ok(el('interaction-definition').textContent.includes('画面の実描画'));
  assert.ok(el('damage').textContent.includes('受けたダメージ'));
  assert.ok(el('damage').textContent.includes('与えたダメージ'));
  assert.ok(el('damage-definition').textContent.includes('攻撃行動の回数をダメージとして代用しません'));
  el('player').value=data.players[0].key;el('player').handlers.change();await flush();
  assert.ok(calls.at(-1).url.includes('player='+data.players[0].key));
  el('period').value='month';el('period').handlers.change();await flush();
  assert.match(el('date').value,/^\d{4}-\d{2}$/);
  el('metric').value='drop_actions';el('metric').handlers.change();await flush();
  assert.ok(calls.at(-1).url.includes('metric=drop_actions'));
  data.selected_player=null;data.totals.dropped_items=null;data.totals.drop_actions=null;
  data.item_drops.types=[];data.ranking=[];el('refresh').handlers.click();await flush();
  assert.ok(el('profile').children.some(c=>c.textContent==='未計測／記録なし'));
  assert.ok(el('types').children[0].textContent.includes('未計測を0'));
  data.player_deaths.causes=[];el('refresh').handlers.click();await flush();
  assert.ok(el('deaths').children[0].textContent.includes('処理前通知から推測しません'));
  data.player_modes.modes=[];el('refresh').handlers.click();await flush();
  assert.ok(el('modes').children[0].textContent.includes('通知だけから数えません'));
  data.item_pickups.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('pickups').children[0].textContent.includes('入口個数から推測しません'));
  data.player_jumps.count=null;el('refresh').handlers.click();await flush();
  assert.equal(el('jump-count').textContent,'未計測／記録なし');
  data.direct_kills.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('kills').children[0].textContent.includes('不明なキラーから推測しません'));
  data.item_use_results.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('use-results').children[0].textContent.includes('試行から成功や消費を推測しません'));
  data.blocks_mined.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('mined').children[0].textContent.includes('破壊試行から推測しません'));
  data.blocks_placed.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('placed').children[0].textContent.includes('設置試行から推測しません'));
  data.items_crafted.types=[];data.travel_distance.modes=[];data.block_interactions.types=[];el('refresh').handlers.click();await flush();
  assert.ok(el('crafted').children[0].textContent.includes('クラフト成立の記録なし'));
  assert.ok(el('travel').children[0].textContent.includes('移動距離の記録なし'));
  assert.ok(el('interactions').children[0].textContent.includes('相互作用の記録なし'));
  el('logout').handlers.click();const count=calls.length;el('refresh').handlers.click();await flush();
  assert.equal(calls.length,count);assert.equal(el('profile').children.length,0);
  assert.equal(el('killed-by').children.length,0);assert.equal(el('killed-by-definition').textContent,'');
  assert.equal(el('projectile-kills').children.length,0);
  console.log('Action DOM PASS: actual API JSON, units/types, literal names, unknown state, filters, token handling; fixture only');
})().catch(error=>{console.error(error);process.exitCode=1;});
