// Real response, actual /actions assets, bounded DOM harness; NOT a browser.
// Output contains only selected count strings, never names/IDs or viewer tokens.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const input=JSON.parse(fs.readFileSync(0,'utf8')),data=input.data,expected=input.expected;
function requireValue(value){if(!value)throw Error('Action DOM mismatch');}
class Element {
  constructor(){this.value='';this.textContent='';this.children=[];this.handlers={};}
  set innerHTML(_){throw Error('Unsafe HTML insertion');}
  append(...values){this.children.push(...values);}replaceChildren(...values){this.children=[...values];}
  addEventListener(name,fn){this.handlers[name]=fn;}
}
const web=path.join(__dirname,'../companion/web'),html=fs.readFileSync(path.join(web,'actions.html'),'utf8');
const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>{requireValue(elements.has('a-'+id));return elements.get('a-'+id);};
el('period').value=data.period.kind;el('metric').value=data.metric;
let calls=0;
let lastUrl='';
vm.runInNewContext(fs.readFileSync(path.join(web,'actions.js'),'utf8'),{
  document:{getElementById:id=>elements.get(id),createElement:()=>new Element()},
  Option:function(text,value){this.textContent=text;this.value=value;},Intl,Date,URLSearchParams,setInterval:()=>0,
  fetch:async(url)=>{++calls;lastUrl=url;return {ok:true,json:async()=>data};}
},{timeout:1000});
requireValue(calls===0);
if(data.period.kind!=='all')el('date').value=data.period.label;
// Dummy DOM-only input, NOT the actual HTTP credential.
el('token').value='dom-only-not-http-credential';el('login').handlers.submit({preventDefault(){}});
setImmediate(()=>{
  el('player').value=data.selected_player.key;el('player').handlers.change();
  setImmediate(()=>{
  try {
    requireValue(el('error').textContent==='' && calls===2 && el('token').value==='');
    const query=new URLSearchParams(lastUrl.split('?')[1]);
    requireValue(query.get('player')===data.selected_player.key && query.get('period')===data.period.kind);
    if(data.period.kind!=='all')requireValue(query.get('date')===data.period.label);
    const fields=new Map(),children=el('profile').children;
    for(let i=0;i<children.length;i+=2)fields.set(children[i].textContent,children[i+1].textContent);
    const p=data.selected_player;requireValue(p && fields.get('対象')===p.name);
    const count=(n,u='回')=>n==null?'未計測／記録なし':n+' '+u;
    const mappings={
      dropped_items:['投棄 true 戻り値・入口個数',count(p.dropped_items,'個')],
      drop_actions:['投棄 true 戻り値',count(p.drop_actions)],
      pickup_inventory_items:['拾得処理中・主所持品の増加',count(p.pickup_inventory_items,'個')],
      pickup_delta_actions:['拾得処理中・所持数増加の観測',count(p.pickup_delta_actions)],
      confirmed_deaths:['死亡フラグ成立（処理後）',count(p.confirmed_deaths)],
      jump_impulses:['ジャンプ処理の上向き加速観測',count(p.jump_impulses)],
      use_return_actions:['使用処理true応答・所持品照合済み',count(p.use_return_actions)],
      use_inventory_loss:['使用true応答中・主所持品の減少',count(p.use_inventory_loss,'個')],
      hurt_slot_losses:['耐久処理true応答・主所持品スロットの1個消失',count(p.hurt_slot_losses)],
      direct_kills:['直接攻撃の死亡フラグ成立観測',count(p.direct_kills)],
      direct_mob_kills:['直接攻撃・Mob／プレイヤー内訳',count(p.direct_mob_kills)+' / '+count(p.direct_player_kills)],
      direct_player_kills:['直接攻撃・Mob／プレイヤー内訳',count(p.direct_mob_kills)+' / '+count(p.direct_player_kills)],
      sprint_starts:['スプリント開始／終了フラグ観測',count(p.sprint_starts)+' / '+count(p.sprint_stops)],
      sprint_stops:['スプリント開始／終了フラグ観測',count(p.sprint_starts)+' / '+count(p.sprint_stops)],
      sneak_starts:['スニーク開始／終了フラグ観測',count(p.sneak_starts)+' / '+count(p.sneak_stops)],
      sneak_stops:['スニーク開始／終了フラグ観測',count(p.sneak_starts)+' / '+count(p.sneak_stops)]
    };
    const metrics={};
    for(const [metric,n] of Object.entries(expected)){
      requireValue(p[metric]===n && mappings[metric]);
      const [label,value]=mappings[metric];requireValue(fields.get(label)===value);metrics[metric]=value;
    }
    // Check actual namespaced type/hand and cause/mode cards too, with no fixture quantities.
    const contains=(id,text)=>el(id).children.some(x=>x.textContent===text);
    for(const t of data.item_drops.types)requireValue(contains('types',t.type+' — '+t.items+' 個 / '+t.actions+' 回'));
    for(const t of data.item_pickups.types)requireValue(contains('pickups',t.type+' — '+t.items+' 個 / '+t.actions+' 回'));
    for(const t of data.direct_kills.types)requireValue(contains('kills',t.type+' / '+(t.kind==='player'?'プレイヤー':'Mob')+' — '+t.count+' 回'));
    for(const t of data.item_use_results.types)requireValue(contains('use-results',t.type+' / '+(t.hand==='main_hand'?'主手':'副手')+' — true応答 '+t.actions+' 回 / 主所持品減少 '+count(t.items,'個')));
    for(const c of data.player_deaths.causes)requireValue(contains('deaths',c.label+' ('+c.code+') — '+c.count+' 回'));
    for(const t of data.item_hurt_slots.types)requireValue(contains('hurt-slots',t.type+' — '+t.count+' 回'));
    for(const m of data.player_modes.modes)requireValue(contains('modes',(m.mode==='sprint'?'スプリント':'スニーク')+' — 開始 '+count(m.starts)+' / 終了 '+count(m.stops)));
    requireValue(el('jump-count').textContent===count(data.player_jumps.count));
    requireValue(el('use-definition').textContent.includes('消費成功ではありません'));
    requireValue(el('hurt-definition').textContent.includes('破損成立・因果関係・永続保存の証明ではありません'));
    el('logout').handlers.click();requireValue(el('profile').children.length===0 && el('use-results').children.length===0);
    console.log(JSON.stringify({verification:'actual Japanese /actions assets DOM, not browser',metrics}));
  }catch(_error){console.error('Action DOM consistency failed');process.exitCode=1;}
  });
});
