// Real authenticated fixture HTTP payloads, including native-writer readback.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const data=JSON.parse(fs.readFileSync(0,'utf8'));
class Element {
  constructor(){this.value='';this.textContent='';this.children=[];this.handlers={};}
  set innerHTML(_){throw Error('Unsafe insertion');}
  append(...values){this.children.push(...values);}replaceChildren(...values){this.children=[...values];}
  addEventListener(event,fn){this.handlers[event]=fn;}
}
function page(name,prefix,payload){
  const html=fs.readFileSync('companion/web/'+name+'.html','utf8');
  const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
  const el=id=>{assert.ok(elements.has(prefix+id),id);return elements.get(prefix+id);};
  const calls=[];
  vm.runInNewContext(fs.readFileSync('companion/web/'+name+'.js','utf8'),{
    document:{getElementById:id=>elements.get(id),createElement:()=>new Element()},
    Option:function(text,value){this.textContent=text;this.value=value;},Intl,Date,URLSearchParams,setInterval:()=>0,
    fetch:async(url,options)=>{calls.push({url,options});return {ok:true,status:200,json:async()=>payload};},
  });
  el('period').value='all';el('token').value='synthetic-only-not-a-real-secret';
  if(name==='actions')el('metric').value='travel_cm';else el('category').value='custom';
  el('login').handlers.submit({preventDefault(){}});
  return {el,calls};
}
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  const a=page('actions','a-',data.actions);await flush();
  for(const mode of data.actions.travel_distance.modes){
    assert.ok(a.el('travel').children.some(x=>x.textContent===mode.mode+' — '+mode.cm+' cm'));
  }
  assert.ok(a.el('travel-definition').textContent.includes('v2'));
  assert.ok(a.el('travel-definition').textContent.includes('全移動距離'));
  const s=page('stats','s-',data.stats);await flush();
  s.el('key').value='minecraft:walk_one_cm';s.el('key').handlers.change();await flush();
  assert.ok(s.calls.at(-1).url.includes('key=minecraft%3Awalk_one_cm'));
  assert.ok(s.el('ranking').children[0].textContent.includes(data.expected_walk+' cm'));
  assert.equal(s.el('types').children[0].children[0].textContent,'歩行距離');
  assert.ok(s.el('definition').textContent.includes('v2'));
  assert.ok(s.el('source').textContent.includes('poll_position_delta.v2'));
  for(const entry of [...a.calls,...s.calls]){
    assert.equal(entry.options.credentials,'omit');assert.ok(!entry.url.includes('synthetic-only'));
  }
  a.el('logout').handlers.click();s.el('logout').handlers.click();
  assert.equal(a.el('travel').children.length,0);assert.equal(s.el('types').children.length,0);
  console.log('Travel v2 DOM PASS: actual fixture HTTP JSON, cm/mode/ranking, v1/v2 provenance, definitions, logout; not gameplay');
})().catch(error=>{console.error(error);process.exitCode=1;});
