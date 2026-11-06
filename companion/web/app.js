(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  let token = '', page = 0, playersPage = 0, selected = '', serial = 0;
  const jst = ms => ms == null ? '記録なし' : new Date(ms).toLocaleString('ja-JP', {timeZone:'Asia/Tokyo'}) + ' JST';
  const set = (id, value) => { $(id).textContent = String(value); };
  function dateDefault() {
    const now = new Intl.DateTimeFormat('sv-SE', {timeZone:'Asia/Tokyo',year:'numeric',month:'2-digit',day:'2-digit'}).format(new Date());
    $('date').type = $('period').value === 'month' ? 'month' : 'date';
    $('date').disabled = $('period').value === 'all';
    $('date').value = $('period').value === 'month' ? now.slice(0,7) : now;
  }
  function profile(p) {
    $('profile').replaceChildren();
    const fields = p ? {'名前':p.name,'確定プレイ時間':p.playtime_text,'開始セッション':p.sessions,
      '破壊の試行':p.destroy_attempts,'設置の試行':p.place_attempts,'参加 / 退出':`${p.joins} / ${p.leaves}`,
      '未完了（全履歴）':p.open_sessions,'不正時間の除外（全履歴）':p.invalid_sessions,
      '最終イベント観測':jst(p.last_active_ms),'現在オンライン':'不明（未完了だけでは判定しません）'} : {'状態':'プレイヤーを選択してください'};
    Object.entries(fields).forEach(([key,value]) => { const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=String(value);$('profile').append(dt,dd); });
  }
  function clear() {
    ++serial; token=''; selected=''; page=0;playersPage=0;$('token').value='';
    ['total-time','total-sessions','total-attempts','open-sessions','peak'].forEach(id=>set(id,'—'));
    ['unsupported','updated','semantics','error'].forEach(id=>set(id,''));
    ['tps','mspt','online'].forEach(id=>set(id,'未計測'));
    $('player').replaceChildren(new Option('選択してください','')); $('rankings').replaceChildren(); profile(null);
    set('period-label','認証後に表示します');set('page','1');set('players-page','1');
  }
  async function refresh() {
    if(!token) return;
    const request=++serial;
    const q=new URLSearchParams({period:$('period').value,metric:$('metric').value,page:String(page),limit:'20',players_page:String(playersPage)});
    if($('period').value!=='all') q.set('date',$('date').value);
    if(selected) q.set('player',selected);
    try {
      const response=await fetch('/api/v1/analytics?'+q,{headers:{Authorization:'Bearer '+token},cache:'no-store',credentials:'omit'});
      const data=await response.json(); if(request!==serial) return;
      if(!response.ok) throw new Error(data.error || '読み取り失敗');
      set('error','');set('period-label',data.period.label+' / '+data.period.timezone);
      set('total-time',data.totals.playtime_text);set('total-sessions',data.totals.sessions);
      set('total-attempts',`${data.totals.destroy_attempts} / ${data.totals.place_attempts}`);set('open-sessions',data.totals.open_sessions);
      $('player').replaceChildren(new Option('選択してください',''));
      data.players.forEach(p=>$('player').append(new Option(p.name,p.key)));
      if(selected && !data.players.some(p=>p.key===selected) && data.selected_player) $('player').append(new Option(data.selected_player.name,selected));
      $('player').value=selected; profile(data.selected_player);
      set('unsupported','未計測: '+data.unmeasured.join('・'));
      $('rankings').replaceChildren();
      data.ranking.forEach(r=>{const li=document.createElement('li'),name=document.createElement('span'),value=document.createElement('strong');name.textContent=`${r.rank}. ${r.name}`;value.textContent=r.display;li.append(name,value);$('rankings').append(li);});
      if(!data.ranking.length){const li=document.createElement('li');li.textContent='この期間の正の記録はありません。未計測の指標は順位を作りません。';$('rankings').append(li);}
      set('page',page+1);$('prev').disabled=page===0;$('next').disabled=data.ranking.length<20;
      set('players-page',playersPage+1);$('players-prev').disabled=playersPage===0;$('players-next').disabled=data.players.length<100;
      set('tps',data.server.effective_tps == null ? '未計測' : data.server.effective_tps);
      set('mspt',data.server.processing_mspt == null ? '未計測' : data.server.processing_mspt);
      set('online',data.server.current_online == null ? '未計測' : data.server.current_online);
      set('peak',data.server.peak_closed_lower_bound == null ? '未集計' : data.server.peak_closed_lower_bound+' 人以上');
      set('sampling',data.server.reason+' 有効サンプル: '+data.server.sample_count+' / 最終サンプル: '+jst(data.server.last_sample_ms));
      set('updated','読取: '+jst(data.generated_at_ms)+' / 最終イベント: '+jst(data.last_event_ms)+'（無イベントは停止・無人の証拠ではありません）');
      set('semantics',data.semantics);
    } catch(error) { if(request===serial) set('error','表示は前回成功時の値の可能性があります: '+error.message); }
  }
  $('login').addEventListener('submit',e=>{e.preventDefault();const submitted=$('token').value.trim();clear();token=submitted;refresh();});
  $('logout').addEventListener('click',clear);$('refresh').addEventListener('click',refresh);
  $('period').addEventListener('change',()=>{dateDefault();page=0;refresh();});
  $('date').addEventListener('change',()=>{page=0;refresh();});$('metric').addEventListener('change',()=>{page=0;refresh();});
  $('player').addEventListener('change',()=>{selected=$('player').value;refresh();});
  $('prev').addEventListener('click',()=>{page=Math.max(0,page-1);refresh();});$('next').addEventListener('click',()=>{page++;refresh();});
  $('players-prev').addEventListener('click',()=>{playersPage=Math.max(0,playersPage-1);refresh();});$('players-next').addEventListener('click',()=>{playersPage++;refresh();});
  dateDefault();setInterval(refresh,30000);
})();
