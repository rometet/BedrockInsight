// Render an authenticated report with the actual Japanese assets. DOM-only,
// not a real browser/screenshot. stdin is report JSON, never a viewer token.
const assert=require('node:assert/strict'), fs=require('node:fs'), vm=require('node:vm');
const path=require('node:path');
const report=JSON.parse(fs.readFileSync(0,'utf8'));
assert.equal(report.server.sampling_status,'measured');
assert.equal(typeof report.server.processing_mspt,'number');
assert.equal(report.server.measurement_method,'Level::$tick/Highest+Lowest');
class Element {
  constructor(){this.value='';this.textContent='';this.children=[];this.handlers={};}
  set innerHTML(_){throw Error('Unsafe HTML insertion');}
  append(...items){this.children.push(...items);}
  replaceChildren(...items){this.children=[...items];}
  addEventListener(name,handler){this.handlers[name]=handler;}
}
const web=path.join(__dirname,'../companion/web');
const html=fs.readFileSync(path.join(web,'index.html'),'utf8');
assert.match(html,/実効TPS/);assert.match(html,/処理時間\s*MSPT/);
const elements=new Map([...html.matchAll(/id="([^"]+)"/g)].map(m=>[m[1],new Element()]));
const el=id=>{assert.ok(elements.has(id));return elements.get(id);};
el('period').value='all';el('metric').value='playtime';
vm.runInNewContext(fs.readFileSync(path.join(web,'app.js'),'utf8'),{
  document:{getElementById:el,createElement:()=>new Element()},
  Option:function(text,value){this.textContent=text;this.value=value;},
  URLSearchParams,Intl,Date,setInterval:()=>0,
  fetch:async()=>({ok:true,json:async()=>report})
});
el('token').value='dom-only-dummy-not-http-credential';
el('login').handlers.submit({preventDefault(){}});
setImmediate(()=>{
  try {
    assert.equal(el('error').textContent,'');
    assert.equal(el('mspt').textContent,String(report.server.processing_mspt));
    assert.equal(el('tps').textContent,String(report.server.effective_tps));
    assert.equal(el('online').textContent,report.server.current_online==null?'未計測':String(report.server.current_online));
    assert.ok(el('sampling').textContent.includes(report.server.reason));
    assert.ok(el('sampling').textContent.includes('Level::$tick'));
    console.log(JSON.stringify({verification:'actual Japanese assets DOM, not browser',
      mspt:el('mspt').textContent,tps:el('tps').textContent,
      online:el('online').textContent,sampling:el('sampling').textContent}));
  } catch(error){console.error(error);process.exitCode=1;}
});
