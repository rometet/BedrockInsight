(() => {
  'use strict';
  const $ = id => document.getElementById('a-'+id);
  const set = (id,value) => { $(id).textContent=String(value); };
  const stamp = ms => ms == null ? '観測なし' : new Date(ms).toLocaleString('ja-JP',{timeZone:'Asia/Tokyo'})+' JST';
  const count = (value,unit='回') => value == null ? '未計測／記録なし' : value+' '+unit;
  let token='',selected='',page=0,playersPage=0,serial=0;
  function dates() {
    const now=new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Tokyo',year:'numeric',month:'2-digit',day:'2-digit'}).format(new Date());
    $('date').type=$('period').value==='month'?'month':'date';
    $('date').disabled=$('period').value==='all';
    $('date').value=$('period').value==='month'?now.slice(0,7):now;
  }
  function clear() {
    ++serial;token='';selected='';page=0;playersPage=0;$('token').value='';
    ['error','definition','observed','source','note','death-definition','death-observed','mode-definition','mode-observed','pickup-definition','pickup-observed','jump-count','jump-definition','jump-observed','kill-definition','kill-observed','use-definition','use-observed','mined-definition','mined-observed','placed-definition','placed-observed','crafted-definition','travel-definition','interaction-definition','damage','damage-definition','damage-observed'].forEach(id=>set(id,''));
    ['profile','rankings','types','deaths','modes','pickups','kills','use-results','hurt-slots','mined','placed','crafted','travel','interactions'].forEach(id=>$(id).replaceChildren());
    set('hurt-definition','');set('hurt-observed','');
    $('killed-by').replaceChildren();set('killed-by-definition','');set('killed-by-observed','');
    $('projectile-kills').replaceChildren();set('projectile-definition','');
    $('player').replaceChildren(new Option('全プレイヤー',''));
    set('period-label','認証後に表示');set('page',1);set('players-page',1);
  }
  async function refresh() {
    if (!token) return;
    const request=++serial;
    const q=new URLSearchParams({period:$('period').value,metric:$('metric').value,page:String(page),limit:'20',players_page:String(playersPage)});
    if ($('period').value!=='all') q.set('date',$('date').value);
    if (selected) q.set('player',selected);
    try {
      const response=await fetch('/api/v1/analytics?'+q,{headers:{Authorization:'Bearer '+token},credentials:'omit',cache:'no-store'});
      const data=await response.json();if (request!==serial) return;
      if (!response.ok) throw new Error(data.error || '読み取り失敗');
      if (!data.item_drops) throw new Error('このAPIには新しい行動ソースがありません');
      set('error','');set('period-label',data.period.label+' / '+data.period.timezone);
      $('player').replaceChildren(new Option('全プレイヤー',''));
      data.players.forEach(p=>$('player').append(new Option(p.name,p.key)));
      if (selected && !data.players.some(p=>p.key===selected) && data.selected_player)
        $('player').append(new Option(data.selected_player.name,selected));
      $('player').value=selected;
      const p=data.selected_player || data.totals;
      const fields={'対象':data.selected_player ? data.selected_player.name : '全プレイヤー',
        '投棄 true 戻り値・入口個数':count(p.dropped_items,'個'),'投棄 true 戻り値':count(p.drop_actions),
        '拾得処理中・主所持品の増加':count(p.pickup_inventory_items,'個'),
        '拾得処理中・所持数増加の観測':count(p.pickup_delta_actions),
        'ジャンプ処理の上向き加速観測':count(p.jump_impulses),
        '使用処理true応答・所持品照合済み':count(p.use_return_actions),
        '使用true応答中・主所持品の減少':count(p.use_inventory_loss,'個'),
        '耐久処理true応答・主所持品スロットの1個消失':count(p.hurt_slot_losses),
        'ブロック採掘の成功（種類別）':count(p.mined_blocks),
        'ブロック採掘成功の観測回数':count(p.mined_block_actions),
        'ブロック設置の成功（種類別）':count(p.placed_blocks),
        'ブロック設置成功の観測回数':count(p.placed_block_actions),
        'クラフト成立アイテム数（種類別）':count(p.crafted_items,'個'),
        'クラフト成立の回数':count(p.craft_actions),
        '移動距離（cm）':count(p.travel_cm,'cm'),
        'ブロック相互作用':count(p.interactions),
        '受けたダメージ（1/10）':count(p.damage_taken_tenths),
        '与えたダメージ（1/10・実ダメージ）':count(p.damage_dealt_tenths),
        '耐久値による破損（種類別）':count(p.broken_items,'個'),
        '睡眠成立（フラグ遷移）':count(p.sleep_successes),
        'スニーク継続（tick）':count(p.sneak_time_ticks),
        '直接攻撃の死亡フラグ成立観測':count(p.direct_kills),
        '直接攻撃・Mob／プレイヤー内訳':count(p.direct_mob_kills)+' / '+count(p.direct_player_kills),
        '死亡処理通知（死亡確定ではない）':count(p.death_notifications),
        '死亡フラグ成立（処理後）':count(p.confirmed_deaths),'アイテム使用試行（成功ではない）':count(p.item_use_attempts),
        'スプリント開始／終了フラグ観測':count(p.sprint_starts)+' / '+count(p.sprint_stops),
        'スニーク開始／終了フラグ観測':count(p.sneak_starts)+' / '+count(p.sneak_stops)};
      $('profile').replaceChildren();
      Object.entries(fields).forEach(([key,value])=>{const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=value;$('profile').append(dt,dd);});
      set('definition',data.item_drops.definition);
      set('observed',data.item_drops.scope+' / 最初: '+stamp(data.item_drops.observed.first_ms)+' / 最後: '+stamp(data.item_drops.observed.last_ms));
      $('types').replaceChildren();
      data.item_drops.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.items+' 個 / '+t.actions+' 回';$('types').append(li);});
      $('pickups').replaceChildren();
      const pickup=data.item_pickups;
      if(pickup) {
        pickup.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.items+' 個 / '+t.actions+' 回';$('pickups').append(li);});
        set('pickup-definition',pickup.definition);
        set('pickup-observed',pickup.scope+' / 最初: '+stamp(pickup.observed.first_ms)+' / 最後: '+stamp(pickup.observed.last_ms)+' / 保存ソース: '+pickup.source);
      } else {set('pickup-definition','このAPIには拾得処理中の所持数差分がありません。未計測。');set('pickup-observed','');}
      if(!pickup || !pickup.types.length) {
        const li=document.createElement('li');li.textContent='主所持品増加の記録なし（不明）。処理前通知や入口個数から推測しません。';$('pickups').append(li);
      }
      const jump=data.player_jumps;
      if(jump) {
        set('jump-count',count(jump.count));set('jump-definition',jump.definition);
        set('jump-observed',jump.scope+' / 最初: '+stamp(jump.observed.first_ms)+' / 最後: '+stamp(jump.observed.last_ms)+' / 保存ソース: '+jump.source);
      } else {
        set('jump-count','未計測／記録なし');set('jump-definition','このAPIにはジャンプ処理の加速計測がありません。入力通知から推測しません。');set('jump-observed','');
      }
      $('kills').replaceChildren();
      const kills=data.direct_kills;
      $('projectile-kills').replaceChildren();
      const projectile=data.projectile_kills;
      if(projectile) {
        projectile.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' / '+(t.kind==='player'?'プレイヤー':'Mob')+' — '+t.count+' 回';$('projectile-kills').append(li);});
        set('projectile-definition',projectile.definition);
      } else set('projectile-definition','このAPIには子飛び道具の所有者解決済み記録がありません。');
      if(!projectile || !projectile.types.length) {
        const li=document.createElement('li');li.textContent='子飛び道具の帰属は未計測／記録なし。所有者不明から推測しません。';$('projectile-kills').append(li);
      }
      if(kills) {
        kills.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' / '+(t.kind==='player'?'プレイヤー':'Mob')+' — '+t.count+' 回';$('kills').append(li);});
        set('kill-definition',kills.definition);
        set('kill-observed',kills.scope+' / 最初: '+stamp(kills.observed.first_ms)+' / 最後: '+stamp(kills.observed.last_ms)+' / 保存ソース: '+kills.source);
      } else {set('kill-definition','このAPIには処理後の直接攻撃帰属観測がありません。未計測。');set('kill-observed','');}
      if(!kills || !kills.types.length) {
        const li=document.createElement('li');li.textContent='直接攻撃の死亡フラグ成立記録なし（不明）。通知や不明なキラーから推測しません。';$('kills').append(li);
      }
      $('use-results').replaceChildren();
      const use=data.item_use_results;
      if(use) {
        use.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' / '+(t.hand==='main_hand'?'主手':'副手')+' — true応答 '+t.actions+' 回 / 主所持品減少 '+count(t.items,'個');$('use-results').append(li);});
        set('use-definition',use.definition);
        set('use-observed',use.scope+' / 最初: '+stamp(use.observed.first_ms)+' / 最後: '+stamp(use.observed.last_ms)+' / 保存ソース: '+use.source);
      } else {set('use-definition','このAPIには使用true応答の照合記録がありません。試行から成功や消費を推測しません。');set('use-observed','');}
      if(!use || !use.types.length) {
        const li=document.createElement('li');li.textContent='使用true応答の照合記録なし（不明）。試行から成功や消費を推測しません。';$('use-results').append(li);
      }
      $('deaths').replaceChildren();
      $('hurt-slots').replaceChildren();
      const hurt=data.item_hurt_slots;
      if(hurt) {
        hurt.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.count+' 回';$('hurt-slots').append(li);});
        set('hurt-definition',hurt.definition);
        set('hurt-observed',hurt.scope+' / 最初: '+stamp(hurt.observed.first_ms)+' / 最後: '+stamp(hurt.observed.last_ms)+' / 保存ソース: '+hurt.source);
      } else {set('hurt-definition','このAPIには耐久処理中のスロット消失記録がありません。破損数は推測しません。');set('hurt-observed','');}
      if(!hurt || !hurt.types.length) {
        const li=document.createElement('li');li.textContent='耐久処理中のスロット消失記録なし（不明）。全破損数ではありません。';$('hurt-slots').append(li);
      }
      $('mined').replaceChildren();
      const mined=data.blocks_mined;
      if(mined) {
        mined.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.count+' 回';$('mined').append(li);});
        set('mined-definition',mined.definition);
        set('mined-observed',mined.scope+' / 最初: '+stamp(mined.observed.first_ms)+' / 最後: '+stamp(mined.observed.last_ms)+' / 保存ソース: '+mined.source);
      } else {set('mined-definition','このAPIには成功採掘の状態変化記録がありません。破壊試行から成功を推測しません。');set('mined-observed','');}
      if(!mined || !mined.types.length) {
        const li=document.createElement('li');li.textContent='成功採掘の記録なし（不明）。破壊試行から推測しません。';$('mined').append(li);
      }
      $('placed').replaceChildren();
      const placed=data.blocks_placed;
      if(placed) {
        placed.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.count+' 回';$('placed').append(li);});
        set('placed-definition',placed.definition);
        set('placed-observed',placed.scope+' / 最初: '+stamp(placed.observed.first_ms)+' / 最後: '+stamp(placed.observed.last_ms)+' / 保存ソース: '+placed.source);
      } else {set('placed-definition','このAPIには成功設置の状態変化記録がありません。設置試行から成功を推測しません。');set('placed-observed','');}
      if(!placed || !placed.types.length) {
        const li=document.createElement('li');li.textContent='成功設置の記録なし（不明）。設置試行から推測しません。';$('placed').append(li);
      }
      const crafted=data.items_crafted;
      if(crafted) {
        crafted.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' — '+t.items+' 個 / '+t.actions+' 回';$('crafted').append(li);});
        set('crafted-definition',crafted.definition);
      } else set('crafted-definition','クラフト成立の記録なし（不明）。試行や失敗転送から推測しません。');
      if(!crafted || !crafted.types.length) {
        const li=document.createElement('li');li.textContent='クラフト成立の記録なし（不明）。';$('crafted').append(li);
      }
      const travel=data.travel_distance;
      if(travel) {
        travel.modes.forEach(m=>{const li=document.createElement('li');li.textContent=m.mode+' — '+m.cm+' cm';$('travel').append(li);});
        set('travel-definition',travel.definition);
      } else set('travel-definition','移動距離の記録なし（不明）。');
      if(!travel || !travel.modes.length) {
        const li=document.createElement('li');li.textContent='移動距離の記録なし（不明）。';$('travel').append(li);
      }
      const inter=data.block_interactions;
      if(inter) {
        inter.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.key+' — '+t.count+' 回';$('interactions').append(li);});
        set('interaction-definition',inter.definition);
      } else set('interaction-definition','ブロック相互作用の記録なし（不明）。');
      if(!inter || !inter.types.length) {
        const li=document.createElement('li');li.textContent='ブロック相互作用の記録なし（不明）。';$('interactions').append(li);
      }
      const dmg=data.damage;
      if(dmg) {
        const n=x=>x==null?'未計測／記録なし':x;
        set('damage','受けたダメージ（1/10）: '+n(dmg.taken_tenths)+' / 与えたダメージ（1/10）: '+n(dmg.dealt_tenths));
        set('damage-definition',dmg.definition);
        set('damage-observed',dmg.scope+' / 最初: '+stamp(dmg.observed.first_ms)+' / 最後: '+stamp(dmg.observed.last_ms)+' / 保存ソース: '+dmg.source);
      } else {set('damage','ダメージの記録なし（不明）。');set('damage-definition','');set('damage-observed','');}
      const death=data.player_deaths;
      $('killed-by').replaceChildren();
      const killedBy=data.killed_by;
      if(killedBy) {
        killedBy.types.forEach(t=>{const li=document.createElement('li');li.textContent=t.type+' / '+(t.path==='direct'?'直接攻撃':'子飛び道具のsource所有者')+' — '+t.count+' 回';$('killed-by').append(li);});
        set('killed-by-definition',killedBy.definition);
        set('killed-by-observed',killedBy.scope+' / 最初: '+stamp(killedBy.observed.first_ms)+' / 最後: '+stamp(killedBy.observed.last_ms)+' / 保存ソース: '+killedBy.source);
      } else {set('killed-by-definition','このAPIには死亡元エンティティの解決済み記録がありません。');set('killed-by-observed','');}
      if(!killedBy || !killedBy.types.length) {
        const li=document.createElement('li');li.textContent='死亡元の種類は未計測／記録なし。環境・不明原因を種類に置き換えません。';$('killed-by').append(li);
      }
      if (death) {
        death.causes.forEach(c=>{const li=document.createElement('li');li.textContent=c.label+' ('+c.code+') — '+c.count+' 回';$('deaths').append(li);});
        set('death-definition',death.definition);
        set('death-observed',death.scope+' / 最初: '+stamp(death.observed.first_ms)+' / 最後: '+stamp(death.observed.last_ms)+' / 保存ソース: '+death.source);
      } else {
        set('death-definition','このAPIには処理後の死亡フラグ計測がありません。未計測。');
        set('death-observed','');
      }
      if (!death || !death.causes.length) {
        const li=document.createElement('li');li.textContent='死亡フラグ成立の記録なし（不明）。処理前通知から推測しません。';$('deaths').append(li);
      }
      $('modes').replaceChildren();
      const modes=data.player_modes;
      if(modes) {
        modes.modes.forEach(m=>{const li=document.createElement('li');li.textContent=(m.mode==='sprint'?'スプリント':'スニーク')+' — 開始 '+count(m.starts)+' / 終了 '+count(m.stops);$('modes').append(li);});
        set('mode-definition',modes.definition);
        set('mode-observed',modes.scope+' / 最初: '+stamp(modes.observed.first_ms)+' / 最後: '+stamp(modes.observed.last_ms)+' / 保存ソース: '+modes.source);
      } else {
        set('mode-definition','このAPIにはモードフラグ計測がありません。未計測。');set('mode-observed','');
      }
      if(!modes || !modes.modes.length) {
        const li=document.createElement('li');li.textContent='状態フラグの観測記録なし（不明）。通知だけから数えません。';$('modes').append(li);
      }
      $('rankings').replaceChildren();
      data.ranking.forEach(r=>{const li=document.createElement('li');li.textContent=r.rank+'. '+r.name+' — '+r.display+' '+(data.metric_unit==='items'?'個':'回');$('rankings').append(li);});
      for (const [id,rows] of [['types',data.item_drops.types],['rankings',data.ranking]]) if (!rows.length) {
        const li=document.createElement('li');li.textContent='正の記録なし（未計測を0や成功に置き換えません）';$(id).append(li);
      }
      set('page',page+1);$('prev').disabled=page===0;$('next').disabled=data.ranking.length<20 || page>=1000;
      set('players-page',playersPage+1);$('players-prev').disabled=playersPage===0;$('players-next').disabled=data.players.length<100 || playersPage>=1000;
      set('source','保存ソース: '+data.item_drops.source+' / 種類は個数降順→種類名の順');
      set('note',data.activity_note+' 読取: '+stamp(data.generated_at_ms));
    } catch (error) {if (request===serial) set('error','前回の表示の可能性があります: '+error.message);}
  }
  $('login').addEventListener('submit',e=>{e.preventDefault();const input=$('token').value.trim();clear();token=input;refresh();});
  $('logout').addEventListener('click',clear);$('refresh').addEventListener('click',refresh);
  $('period').addEventListener('change',()=>{dates();page=0;refresh();});
  $('date').addEventListener('change',()=>{page=0;refresh();});$('metric').addEventListener('change',()=>{page=0;refresh();});
  $('player').addEventListener('change',()=>{selected=$('player').value;refresh();});
  $('prev').addEventListener('click',()=>{page=Math.max(0,page-1);refresh();});
  $('next').addEventListener('click',()=>{page=Math.min(1000,page+1);refresh();});
  $('players-prev').addEventListener('click',()=>{playersPage=Math.max(0,playersPage-1);refresh();});
  $('players-next').addEventListener('click',()=>{playersPage=Math.min(1000,playersPage+1);refresh();});
  dates();setInterval(refresh,30000);
})();
