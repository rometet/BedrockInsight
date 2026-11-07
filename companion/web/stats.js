(() => {
  'use strict';
  const $=id=>document.getElementById('s-'+id), set=(id,text)=>{$(id).textContent=String(text);};
  const stamp=ms=>ms==null?'観測なし':new Date(ms).toLocaleString('ja-JP',{timeZone:'Asia/Tokyo'})+' JST';
  let token='',serial=0,selected='',page=0,rankPage=0,playersPage=0,catalog=null;
  function dates() {
    const now=new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Tokyo',year:'numeric',month:'2-digit',day:'2-digit'}).format(new Date());
    $('date').type=$('period').value==='month'?'month':'date';$('date').disabled=$('period').value==='all';
    $('date').value=$('period').value==='month'?now.slice(0,7):now;
  }
  function clearResults() {
    ['types','ranking'].forEach(id=>$(id).replaceChildren());
    ['title','scope','total','period-label','definition','source','coverage','updated'].forEach(id=>set(id,'—'));
    set('status','未計測／取得待ち');
    ['prev','next','rank-prev','rank-next','players-prev','players-next'].forEach(id=>$(id).disabled=true);
  }
  function logout() {
    ++serial;token='';selected='';page=0;rankPage=0;playersPage=0;catalog=null;$('token').value='';
    clearResults();set('status','未認証');set('error','');set('limitations','未認証。取得した表示を消去しました。');
    $('player').replaceChildren(new Option('全プレイヤー',''));$('key').replaceChildren(new Option('完結プレイ時間の下限','minecraft:play_time'));
    $('key').disabled=true;['search','type'].forEach(id=>$(id).value='');
  }
  const count=(value,unit)=>value==null?'未計測／記録なし':Number.isSafeInteger(value)?value.toLocaleString('ja-JP')+' '+unit:'表示精度の範囲外（API原文を確認）';
  async function refresh() {
    if(!token)return;
    const request=++serial;clearResults();set('error','');
    const q=new URLSearchParams({category:$('category').value,period:$('period').value,page:String(page),rank_page:String(rankPage),limit:'20',players_page:String(playersPage)});
    if($('category').value==='custom')q.set('key',$('key').value || 'minecraft:play_time');
    if($('period').value!=='all')q.set('date',$('date').value);
    if(selected)q.set('player',selected);
    if($('search').value)q.set('search',$('search').value);
    if($('type').value)q.set('type',$('type').value);
    try {
      const response=await fetch('/api/v1/stats?'+q,{headers:{Authorization:'Bearer '+token},credentials:'omit',cache:'no-store'});
      const data=await response.json();if(request!==serial)return;
      if(response.status===401){logout();set('status','再認証が必要');set('error',data.error || '閲覧トークンが期限切れです');return;}
      if(!response.ok)throw new Error(data.error || '取得できません');
      if(!catalog) {
        catalog=data.catalog;const key=$('key').value || 'minecraft:play_time';
        $('key').replaceChildren(...catalog.custom.map(k=>new Option(k.label+' · '+k.id+(k.observation_stage==='pre_origin_notification'?'（処理前通知）':k.projection_supported?'（限定観測）':'（未対応）'),k.id)));
        $('key').value=key;
      }
      $('key').disabled=$('category').value!=='custom';
      $('player').replaceChildren(new Option('全プレイヤー',''));
      data.players.forEach(p=>$('player').append(new Option(p.name,p.key)));
      if(selected && !data.players.some(p=>p.key===selected) && data.selected_player)$('player').append(new Option(data.selected_player.name,selected));
      $('player').value=selected;
      set('title',data.label+' / '+(data.key || data.category));
      set('status',data.status==='unsupported'?'未対応／未計測（値を推測しません）':data.status==='no_record'?'記録なし／未計測を含む（0とは断定しません）':data.status==='stale_snapshot'?'過去のワールド観測値（現在値ではありません）':data.status==='recorded_snapshot'?'ワールドtickの観測値（実機受入は未証明）':data.observation_stage==='pre_origin_notification'?'処理前申告の観測あり（適用結果ではありません）':'正の観測記録あり（完全収集・実機受入の証明ではありません）');
      if(data.aggregation==='latest_per_player_snapshot' && data.totals.source_rows)set('status','個人別の最終保存観測値（現在値・実機受入の証明ではありません）');
      if(data.observation_stage==='derived_resistance_formula' && data.totals.source_rows)set('status','耐性効果からの計算値（実軽減量・実機受入は未証明）');
      if(data.observation_stage==='health_attribute_delta' && data.totals.source_rows)set('status','HP属性の前後減少を観測（最終結果・実機受入は未証明）');
      set('scope',data.metric_scope==='world'?'ワールド共有値（プレイヤー選択は値を変えません）':data.selected_player?data.selected_player.name:'全プレイヤー');set('total',count(data.totals.value,data.unit));
      set('period-label',data.period.label+' / '+data.period.timezone);set('definition',data.definition);
      set('source','保存ソース: '+(data.sources.join(', ') || '対応する確定結果の読取経路なし'));
      set('coverage','最初: '+stamp(data.totals.first_ms)+' / 最後: '+stamp(data.totals.last_ms)+'。この間の連続収集は保証しません。');
      data.types.forEach(t=>{
        const li=document.createElement('li'),name=document.createElement('span'),id=document.createElement('span'),v=document.createElement('span'),button=document.createElement('button');
        li.className='stat-row';name.textContent=t.label;id.className='stat-id';id.textContent=t.type;v.className='stat-value';v.textContent=count(t.value,data.unit);
        button.type='button';button.className='secondary';button.textContent='この種類で順位を比較';button.addEventListener('click',()=>{$('type').value=t.type;page=0;rankPage=0;refresh();});
        if(data.ranking_supported===false){button.disabled=true;button.textContent='共有値のため順位対象外';}
        li.append(name,id,v,button);$('types').append(li);
      });
      data.ranking.forEach(r=>{const li=document.createElement('li');li.textContent=r.rank+'. '+r.name+' — '+count(r.value,data.unit);$('ranking').append(li);});
      for(const id of ['types','ranking'])if(!$(id).children.length){const li=document.createElement('li');li.textContent=id==='ranking'&&data.ranking_supported===false?data.ranking_scope:'正の記録なし。未対応・未計測を0にしません。';$(id).append(li);}
      set('page',page+1);set('rank-page',rankPage+1);set('players-page',playersPage+1);
      $('prev').disabled=page===0;$('next').disabled=!data.has_more || page>=1000;
      $('rank-prev').disabled=data.ranking_supported===false || rankPage===0;$('rank-next').disabled=data.ranking_supported===false || !data.ranking_has_more || rankPage>=1000;
      $('players-prev').disabled=playersPage===0;$('players-next').disabled=!data.players_has_more || playersPage>=1000;
      set('limitations',data.limitations);set('updated','読取時刻: '+stamp(data.generated_at_ms)+' / DLL実機受入: 未証明');
    }catch(error){if(request===serial){clearResults();set('status','取得失敗');set('error',error.message);}}
  }
  function resetPages(){page=0;rankPage=0;}
  $('login').addEventListener('submit',e=>{e.preventDefault();const input=$('token').value.trim();logout();token=input;refresh();});
  $('logout').addEventListener('click',logout);$('refresh').addEventListener('click',refresh);
  $('category').addEventListener('change',()=>{resetPages();$('type').value='';$('search').value='';$('key').disabled=$('category').value!=='custom';refresh();});
  $('key').addEventListener('change',()=>{resetPages();$('type').value='';refresh();});
  $('period').addEventListener('change',()=>{dates();resetPages();refresh();});$('date').addEventListener('change',()=>{resetPages();refresh();});
  $('player').addEventListener('change',()=>{selected=$('player').value;page=0;refresh();});
  $('filter').addEventListener('click',()=>{resetPages();refresh();});$('reset').addEventListener('click',()=>{$('search').value='';$('type').value='';resetPages();refresh();});
  for(const [id,step,which] of [['prev',-1,'types'],['next',1,'types'],['rank-prev',-1,'rank'],['rank-next',1,'rank'],['players-prev',-1,'players'],['players-next',1,'players']]) {
    $(id).addEventListener('click',()=>{if(which==='types')page=Math.max(0,Math.min(1000,page+step));else if(which==='rank')rankPage=Math.max(0,Math.min(1000,rankPage+step));else playersPage=Math.max(0,Math.min(1000,playersPage+step));refresh();});
  }
  dates();logout();setInterval(refresh,30000);
})();
