'use strict';
const $=id=>document.getElementById(id);
let feedbackDefaults={},feedbackPresets={},configTemplates={},flywheelDefaults={},flywheelPresets={},previewOnly=false,nativeUsb=false;
let token='', config, defaults, selected=0, currentJob=0, jobAction='', dirty=false;
const shortcuts=[['复制','mod',67],['粘贴','mod',86],['剪切','mod',88],['全选','mod',65],['撤销','mod',90],['重做','redo',90],['保存','mod',83],['查找','mod',70],['退格',8],['删除',46],['回车',13],['取消',27],['空格',32],['Tab',9],['上箭头',38],['下箭头',40],['左箭头',37],['右箭头',39]];
function node(tag,cls,text){const n=document.createElement(tag);if(cls)n.className=cls;if(text!==undefined)n.textContent=text;return n;}
function active(){return config.profiles[selected];}
function notice(text,error=false){$('jobTitle').textContent=text;$('jobTitle').classList.toggle('error',error);}
function savedDraft(){try{localStorage.setItem('hapticpad-stable-1.8-draft',JSON.stringify(config));}catch{}}
function changed(){dirty=true;savedDraft();$('editState').textContent='配置已修改 · 尚未写入设备';}
function keyName(k){return ({0:'',8:'Back',9:'Tab',13:'Enter',16:'Shift',17:'Ctrl',18:'Alt',27:'Esc',32:'Space',37:'←',38:'↑',39:'→',40:'↓',46:'Del',91:active().name.startsWith('Mac')?'Cmd':'Win',92:'Win'})[k]??((k>=65&&k<=90)||(k>=48&&k<=57)?String.fromCharCode(k):k>=112&&k<=123?'F'+(k-111):'K'+k);}
function combo(b){return b.actions.map(a=>keyName(a[1])).filter(Boolean).join('+')||'未设置';}
function preset(s){const mac=active().name.startsWith('Mac'),mod=mac?91:17;let codes=s[1]==='mod'?[mod,s[2],0]:s[1]==='redo'?(mac?[91,16,90]:[17,89,0]):[s[1],0,0];return {label:s[0],actions:codes.map(v=>[0,v])};}
function rgb(v){return '#'+v.map(x=>Number(x).toString(16).padStart(2,'0')).join('');}
function render(){
 const p=active();p.led_direction??=1;p.led_offset??=0;p.feedback={...feedbackDefaults,...p.feedback};p.flywheel={...flywheelDefaults,...p.flywheel};$('profiles').replaceChildren();
 config.profiles.forEach((profile,i)=>{const b=node('button',i===selected?'selected':'',profile.name);b.onclick=()=>{selected=i;render();};$('profiles').append(b);});
 $('displayScene').value=p.display_scene??'Dog';$('displayText').disabled=$('displayScene').value!=='Text';
 $('displayText').value=p.display_text??'';displayTextCount();
 $('profileName').value=p.name;$('motorEnabled').checked=config.motor_enabled;
 
 $('deleteProfile').disabled=config.profiles.length===1;
 $('keys').replaceChildren();
 p.buttons.forEach((b,i)=>$('keys').append(bindingCard(b,String(i+1),v=>p.buttons[i]=v)));
 renderChords();
 for(const key of ['mode','direction','led','brightness'])$(key).value=p[key];
 const wheel=$('wheelKey');wheel.querySelectorAll('[data-custom]').forEach(x=>x.remove());
 if(![...wheel.options].some(x=>Number(x.value)===p.wheel_key)){const opt=new Option('自定义 VK '+p.wheel_key,String(p.wheel_key));opt.dataset.custom='1';wheel.add(opt);}wheel.value=p.wheel_key;
 for(const key of ['primary','secondary'])$(key).value=rgb(p[key]);
 $('brightnessValue').value=p.brightness;ledHelp();modeHelp();renderFeedback();renderFlywheel();
 $('gains').replaceChildren();
 for(const [key,value]of Object.entries(config.gains)){const l=node('label','',key),input=node('input');input.type='number';input.min=0;input.max=5;input.step='.01';input.value=value;input.onchange=()=>{config.gains[key]=Number(input.value);changed();};l.append(input);$('gains').append(l);}
}
function ensureChords(p){
 p.aux_mode??='legacy';p.chord_window_ms??=100;p.chords??=[];
 p.extra_buttons??=[7,8].map(i=>({label:'小键'+i,actions:[[0,0],[0,0],[0,0]]}));
}
function maskName(mask){return Array.from({length:8},(_,i)=>mask&(1<<i)?String(i+1):null).filter(Boolean).join('+');}
function bitCount(mask){let n=0;for(;mask;mask&=mask-1)n++;return n;}
function bindingCard(b,title,setter,disabled=false){
 const card=node('div','key'),top=node('div','key-top'),c=node('span','combo',combo(b));top.append(node('span','key-number',title),c);
 const labelInput=node('input');labelInput.value=b.label;labelInput.setAttribute('aria-label',`${title} 中文名称`);labelInput.maxLength=31;labelInput.oninput=()=>{b.label=labelInput.value;changed();};
 const choice=node('select');choice.setAttribute('aria-label',`${title} 快捷操作`);choice.add(new Option('自定义快捷键','custom'));
 shortcuts.forEach((s,j)=>{choice.add(new Option(s[0],String(j)));if(JSON.stringify(preset(s).actions)===JSON.stringify(b.actions))choice.value=String(j);});
 choice.onchange=()=>{if(choice.value==='custom')return;Object.assign(b,preset(shortcuts[Number(choice.value)]));setter(b);changed();render();};
 const details=node('details'),summary=node('summary','','高级：动作与延时');details.append(summary,node('p','hint','修饰键放前，字母放后；空槽为0。每项延时0～2000ms。'));
 b.actions.forEach((a,j)=>{const row=node('div','action-row');['延时 ms','按键码 VK'].forEach((name,k)=>{const l=node('label','',`${j+1}. ${name}`),input=node('input');input.type='number';input.min='0';input.max=k===0?'2000':'255';input.value=a[k];input.onchange=()=>{a[k]=Number(input.value);c.textContent=combo(b);choice.value='custom';changed();};l.append(input);row.append(l);});details.append(row);});
 const record=node('button','record-shortcut','录入快捷键');record.setAttribute('aria-label',`为 ${title} 录入快捷键`);record.onclick=()=>openRecorder(b,title);
 card.append(top,labelInput,record,choice,details);
 if(disabled){card.classList.add('inactive-binding');card.querySelectorAll('input,button,select').forEach(el=>el.disabled=true);}
 return card;
}
function renderChords(){
 const p=active();ensureChords(p);$('auxMode').value=p.aux_mode;$('chordWindow').value=p.chord_window_ms;
 const legacy=p.aux_mode==='legacy';
 $('auxHelp').textContent=legacy?'7短按下一配置，8短按上一配置；长按7进入配置选择，长按8进入设置。7、8的自定义绑定暂不生效，可切换模式启用。':p.aux_mode==='custom'?'7、8短按可自定义。7＋8短按可绑定动作；长按约0.8秒进入设置，短按动作会等松手后触发。':'8个按键和全部255种组合可自定义。按键不再进入菜单或切配置；可先保留其他配置的原功能模式，需要切换时可在网页读取、调整顺序并写入。';
 $('extraKeys').replaceChildren(...p.extra_buttons.map((b,i)=>bindingCard(b,String(i+7),v=>p.extra_buttons[i]=v,legacy)));
 $('chordPicker').replaceChildren();
 for(let i=1;i<=8;i++){
  const label=node('label','chord-choice'),input=node('input');input.type='checkbox';input.value=String(i);input.setAttribute('aria-label',`组合选择第${i}键`);input.onchange=updateChordSelection;
  label.append(input,document.createTextNode(String(i)));$('chordPicker').append(label);
 }
 updateChordSelection();
 $('chordCapacity').textContent=`已配置 ${p.chords.length} 个多键组合。当前模式可用 ${legacy?'6个单键＋57个多键组合＝63':'8个单键＋247个多键组合＝255'} 种；未启用的7、8相关绑定会保留。`;
 $('chordList').replaceChildren();let visible=0;
 for(const b of [...p.chords].sort((a,b)=>bitCount(a.mask)-bitCount(b.mask)||a.mask-b.mask)){
  if($('chordFilter').value!=='all'&&bitCount(b.mask)!==Number($('chordFilter').value))continue;
  const card=bindingCard(b,maskName(b.mask),()=>{});
  if(legacy&&(b.mask&192))card.append(node('p','hint','暂停：需启用7、8自定义'));
  const remove=node('button','danger','删除这个组合');remove.onclick=()=>{p.chords=p.chords.filter(x=>x!==b);changed();renderChords();};card.append(remove);$('chordList').append(card);visible++;
 }
 if(!visible)$('chordList').append(node('p','hint',p.chords.length?'此分类暂无组合。':'尚未设置多键组合，原有单键仍可使用。'));
}
function pickedMask(){return [...$('chordPicker').querySelectorAll('input:checked')].reduce((mask,input)=>mask|(1<<(Number(input.value)-1)),0);}
function updateChordSelection(){const mask=pickedMask();$('chordSelection').textContent=bitCount(mask)>1?'设备组合：'+maskName(mask):'请选择至少两个键';}
$('auxMode').onchange=()=>{ensureChords(active());active().aux_mode=$('auxMode').value;changed();renderChords();};
$('chordWindow').onchange=()=>{active().chord_window_ms=Number($('chordWindow').value);changed();};
$('chordFilter').onchange=renderChords;
$('addChord').onclick=()=>{
 const p=active(),mask=pickedMask();if(bitCount(mask)<2)return notice('至少选择两个设备按键',true);
 if(p.chords.some(b=>b.mask===mask))return notice('这个组合已存在，请在下方编辑',true);
 const binding={mask,label:'组合'+maskName(mask),actions:[[0,0],[0,0],[0,0]]};p.chords.push(binding);$('chordFilter').value='all';changed();renderChords();openRecorder(binding,maskName(mask));
};
const feedbackFields={feedbackStrength:'strength',feedbackDamping:'damping',feedbackAssist:'assist',feedbackVoltage:'voltage',feedbackDetents:'detents',feedbackSpeed:'speed_rpm',feedbackDuration:'assist_ms',feedbackTrigger:'trigger_rpm'};
function renderFeedback(editingId=null){
 const f=active().feedback,picker=$('feedbackPreset');picker.replaceChildren(new Option('自定义','custom'));
 for(const [key,preset]of Object.entries(feedbackPresets)){
  picker.add(new Option(preset.label,key));
  if(active().mode===preset.wheel_mode&&Object.keys(feedbackDefaults).every(field=>f[field]===preset.feedback[field]))picker.value=key;
 }
 $('feedbackMode').value=f.mode;$('feedbackDetents').disabled=$('feedbackSpeed').disabled=false;
 for(const [id,key]of Object.entries(feedbackFields)){if(id!==editingId)$(id).value=f[key];if($(id+'Value'))$(id+'Value').value=f[key]+(key==='detents'?' 格':' / 100');}
 $('feedbackStrength').disabled=f.mode!=='Follow';$('feedbackAssist').disabled=f.mode!=='Assist';
 $('feedbackDamping').disabled=f.mode==='Free';$('feedbackVoltage').disabled=f.mode==='Free';
 $('feedbackDuration').disabled=$('feedbackTrigger').disabled=f.mode!=='Assist';
 $('feedbackHelp').textContent=({Follow:'力度控制刻度、回中或惯性的响应；阻尼增加旋转阻力。每圈刻度数决定格子疏密，输出上限约束电机驱动幅度。',Damping:'平滑阻尼只抵抗转动，不会把旋钮拉回某个角度。阻尼越高，转起来越沉。',Assist:'短时助转：停稳后，开始转动时顺方向帮推一下。推动到时、反向或超速立即停止；需停稳后才能再次触发，不是持续自动旋转。',Free:'电机不提供力反馈，旋钮仍会正常输出滚动。'})[f.mode];
}
$('feedbackPreset').onchange=()=>{
 const preset=feedbackPresets[$('feedbackPreset').value];if(!preset)return;
 active().feedback=structuredClone(preset.feedback);active().mode=preset.wheel_mode;$('mode').value=preset.wheel_mode;
 changed();modeHelp();renderFeedback();renderFlywheel();notice('手感预设已调整，写入 SD 后生效');
};
$('feedbackMode').onchange=()=>{
 active().feedback.mode=$('feedbackMode').value;
 if(active().feedback.mode!=='Follow'){active().mode='Clicky';$('mode').value='Clicky';}
 changed();modeHelp();renderFeedback();renderFlywheel();
};
for(const [id,key]of Object.entries(feedbackFields))$(id).oninput=()=>{active().feedback[key]=$(id).value===''?NaN:Number($(id).value);changed();renderFeedback(id);renderFlywheel();};
let recordProfile=null,recordBinding=null,recordTitleName='',recordCodes=null,recordLabelEdited=false;
function resetRecorder(){
 recordCodes=null;$('recordPreview').textContent='请按下快捷键…';$('recordApply').disabled=true;
 $('recordHint').textContent='按下后松开，核对组合键再确认。Esc 也可以作为快捷键录入。';
 $('recordHint').classList.remove('error');$('capturePad').focus();
}
function openRecorder(binding,title){
 recordProfile=active();recordBinding=binding;recordTitleName=title;recordLabelEdited=false;
 $('recordTitle').textContent=`为设备 ${title} 录入快捷键`;
 $('recordLabel').value=binding.label;
 $('recordDialog').showModal();resetRecorder();
}
$('capturePad').addEventListener('keydown',event=>{
 event.preventDefault();event.stopPropagation();if(event.repeat)return;
 try{
  const result=ShortcutCapture.fromEvent(event);recordCodes=result.complete?result.codes:null;
  $('recordPreview').textContent=result.codes.map(keyName).join(' + ')+(result.complete?'':' + …');
  $('recordApply').disabled=!result.complete;$('recordHint').classList.remove('error');
  $('recordHint').textContent=result.complete?'已录入。可以修改中文名称，点击确认保存到这一键。':'继续按下一个字母或功能键…';
  if(result.complete&&!recordLabelEdited)$('recordLabel').value=ShortcutCapture.guessLabel(result.codes);
 }catch(e){recordCodes=null;$('recordApply').disabled=true;$('recordPreview').textContent='请重新按下快捷键';$('recordHint').textContent=e.message;$('recordHint').classList.add('error');}
});
$('recordLabel').oninput=()=>{recordLabelEdited=true;};
$('recordCancel').onclick=()=>$('recordDialog').close();
$('recordAgain').onclick=resetRecorder;
$('recordApply').onclick=()=>{
 if(!recordCodes)return;
 const label=$('recordLabel').value.trim();
 if(!label||new TextEncoder().encode(label).length>31||/[\x00-\x1f]/.test(label)){
  $('recordHint').textContent='名称不能为空，最多31个UTF-8字节（约10个汉字）。';$('recordHint').classList.add('error');return;
 }
 if(!config.profiles.includes(recordProfile)||![...recordProfile.buttons,...(recordProfile.extra_buttons||[]),...(recordProfile.chords||[])].includes(recordBinding)){$('recordHint').textContent='配置刚刚发生更新，请取消并重新录入。';return;}
 const codes=[...recordCodes];while(codes.length<3)codes.push(0);
 Object.assign(recordBinding,{label,actions:codes.map(v=>[0,v])});
 $('recordDialog').close();changed();render();notice(`设备 ${recordTitleName} 已设置为 ${combo(recordBinding)}，写入 SD 后生效`);
 document.querySelector('.record-shortcut')?.focus();
};
function ledHelp(){
 const trail=active().led==='KnobTrail',follow=active().led==='Knob'||trail;$('knobLedOptions').hidden=!follow;
 $('ledDirection').value=active().led_direction??1;$('ledOffset').value=(active().led_offset??0)+1;
 $('primaryLabel').textContent=trail?'亮点颜色 · 自动随机':follow?'亮点颜色':'主色';$('primary').disabled=trail;$('secondaryLabel').textContent=follow?'暗色背景':'辅助色';
}
$('ledDirection').onchange=()=>{active().led_direction=Number($('ledDirection').value);changed();};
$('ledOffset').onchange=()=>{active().led_offset=Number($('ledOffset').value)-1;changed();};
const flywheelFields={flyDuration:'duration_ms',flyHalfLife:'half_life_ms',flyVoltage:'voltage',flyMax:'max_rpm',flyTrigger:'trigger_rpm',flyBrake:'brake_percent',flyBrakeMs:'brake_ms',flyCapture:'capture_ms',flyBoost:'boost_percent'};
function renderFlywheel(editingId=null){
 const p=active();p.flywheel={...flywheelDefaults,...p.flywheel};$('flywheelSettings').hidden=p.mode!=='Flywheel';
 const picker=$('flywheelPreset');picker.replaceChildren(new Option('自定义','custom'));
 for(const [key,preset]of Object.entries(flywheelPresets)){picker.add(new Option(preset.label,key));if(Object.keys(flywheelDefaults).every(field=>p.flywheel[field]===preset.settings[field]))picker.value=key;}
 for(const [id,key]of Object.entries(flywheelFields))if(id!==editingId)$(id).value=p.flywheel[key];
 if(p.mode==='Flywheel'){
  $('feedbackHelp').textContent='无极模式使用速度补偿，不产生刻度或回中；力度缩放速度控制P/I。旋转阻尼、刻度密度和短时助转参数不参与续转。独立设置在下面的“无极续转”区域。';
  for(const id of ['feedbackDamping','feedbackDetents','feedbackSpeed','feedbackAssist','feedbackDuration','feedbackTrigger'])$(id).disabled=true;
 }
}
for(const [id,key]of Object.entries(flywheelFields))$(id).oninput=()=>{active().flywheel[key]=$(id).value===''?NaN:Number($(id).value);changed();renderFlywheel(id);};
$('flywheelPreset').onchange=()=>{const preset=flywheelPresets[$('flywheelPreset').value];if(!preset)return;active().flywheel=structuredClone(preset.settings);changed();renderFlywheel();notice('已调整本地续转参数，未操作设备');};
function modeHelp(){$('modeHelp').textContent=({Clicky:'刻度模式：每格输出一次滚动。打开力反馈后会有刻度手感，适合日常使用。',Twist:'回中模式：旋钮偏离中心时持续滚动，松手后会拉回中心。这是此模式的设计行为。',Flywheel:'无极续转：拨动实体旋钮后，电机在限定时间内补偿阻力；目标转速逐渐衰减。实际续转效果取决于电机与参数。',Momentum:'惯性模式：快速转动后保留转速并逐渐衰减。初次使用建议先测试刻度模式。'})[active().mode];}
async function api(action,payload={}){const r=await fetch('/api/'+action,{method:'POST',headers:{'Content-Type':'application/json','X-Hapticpad-Token':token},body:JSON.stringify(payload)});const data=await r.json();if(!r.ok)throw Error(data.error||'请求失败');return data;}
function setBusy(value){document.querySelectorAll('[data-hardware]').forEach(b=>b.disabled=value||previewOnly||(!nativeUsb&&!['chooseFirmware','useBuiltin','uf2'].includes(b.id)));$('busy').textContent=value?'处理中…':'空闲';$('activity').hidden=!value;}
async function start(action,payload={}){try{if(previewOnly)throw Error('此模式未启用设备操作');setBusy(true);jobAction=action;$('log').textContent='正在开始操作…';notice(({flash:'正在刷写固件',write:'正在写入 SD 配置',read:'正在读取 SD 配置',motor:'正在切换力反馈',status:'正在检测设备',resume:'正在恢复键盘和鼠标'})[action]);const result=await api(action,payload);currentJob=result.id;poll();}catch(e){setBusy(false);notice(e.message,true);$('log').textContent=e.message;}}
async function poll(){try{const job=await(await fetch('/api/job')).json();if(job.id!==currentJob)throw Error('操作编号已变化，请重新检测设备');$('log').textContent=job.logs.join('\n')||'正在与设备通信…';if(job.state==='running'){setTimeout(poll,700);return;}setBusy(false);if(job.state==='error'){notice('操作未完成',true);return;}
 const result=job.result||{};
 if(jobAction==='status')showStatus(result);
 if(jobAction==='flash'){if(result.device)showStatus(result.device);notice(result.device?'固件已刷入并校验通过':'写入已校验，等待检查设备');}
 else if(jobAction==='read'){savedDraft();try{localStorage.setItem('hapticpad-stable-1.8-before-read',JSON.stringify(config));}catch{}config=result.config;selected=0;dirty=false;savedDraft();render();$('editState').textContent='已读取设备 SD 配置；原编辑内容已保留为本机读取前快照。';notice('SD 配置已读取');}
 else if(jobAction==='write'){notice(result.resumed?'SD 已写入，键盘和鼠标已恢复':'SD 已写入，请拔插 USB 恢复使用',!result.resumed);$('editState').textContent='已写入提交时的配置。操作期间如继续修改，请再次写入。';}
 else if(jobAction==='resume')notice(result.resumed?'已恢复正常使用':'尚未恢复，请拔插 USB',!result.resumed);
 else if(jobAction==='motor')notice('力反馈开关已发送');
 if(result.device&&jobAction!=='flash')showStatus(result.device);
 if(result.backup)$('log').textContent+='\n备份位置：'+result.backup;
 if(result.message)$('log').textContent+='\n'+result.message;
 }catch(e){setBusy(false);notice('连接中断：'+e.message,true);}}
function showStatus(s){const titles={ready:s.info?.disk_mode?'U盘配置模式 · 按键暂停':'设备已连接',legacy:'已连接 · 串口未回应',boot:'BOOTSEL · 可以刷机',disconnected:'设备未连接'};$('deviceState').textContent=titles[s.state]||'未知状态';$('deviceDetail').textContent=s.info?`${s.info.version} · ${s.info.profile||'未加载配置'} · 力反馈${s.info.motor_requested?'开启':'关闭'} · SD${s.info.sd?'正常':'未就绪'}${s.info.disk_mode?' · 安全弹出后退出U盘模式':s.info.keyboard_ready!==undefined?` · 键盘${s.info.keyboard_ready?'就绪':'未就绪'} / 鼠标${s.info.mouse_ready?'就绪':'未就绪'}`:''}`:s.message||'';if(jobAction==='status')notice(titles[s.state]||'检测完成');}
function download(name,text){const url=URL.createObjectURL(new Blob([text],{type:'application/xml;charset=utf-8'})),a=node('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
const inputEvents=[],inputKeyEvents=[];let inputKeyCount=0,inputWheelCount=0;
function inputCounts(){$('inputCounts').textContent=`按键 ${inputKeyCount} 次 · 滚动 ${inputWheelCount} 次`;}
$('inputCheck').onclick=()=>{$('inputCheck').focus();$('inputCheck').textContent='正在检测 · 按设备大键，或在这里转旋钮';};
$('inputCheck').onkeydown=e=>{
 e.preventDefault();e.stopPropagation();if(e.repeat||['Meta','Control','Alt','Shift'].includes(e.key))return;
 const mods=[e.metaKey?'Cmd':'',e.ctrlKey?'Ctrl':'',e.altKey?'Alt':'',e.shiftKey?'Shift':''].filter(Boolean);
 const key=e.code.replace(/^Key|^Digit/,'');
 const label=({Backspace:'退格',Delete:'删除',Enter:'回车'})[key]||((e.metaKey||e.ctrlKey)?({C:'复制',V:'粘贴',X:'剪切',A:'全选',Z:'撤销'})[key]:'');
 const text=[...mods,key].join(' + ')+(label?'（'+label+'）':'');
 $('lastInputKey').textContent='最近按键：'+text;inputKeyCount++;inputCounts();
 inputKeyEvents.unshift(new Date().toLocaleTimeString()+' '+text);$('inputKeyEvents').textContent=inputKeyEvents.slice(0,6).join('\n');
};
$('inputCheck').addEventListener('wheel',e=>{e.preventDefault();inputWheelCount++;inputCounts();inputEvents.unshift(new Date().toLocaleTimeString()+' 滚动：'+(e.deltaY<0?'向上':e.deltaY>0?'向下':'横向')+' ΔY='+e.deltaY.toFixed(2)+' ΔX='+e.deltaX.toFixed(2));$('inputEvents').textContent=inputEvents.slice(0,4).join('\n');},{passive:false});
$('clearInputCheck').onclick=()=>{inputEvents.length=inputKeyEvents.length=0;inputKeyCount=inputWheelCount=0;inputCounts();$('lastInputKey').textContent='最近按键：尚未收到';$('inputKeyEvents').textContent='按键记录保留在这里，不会被滚动刷走。';$('inputEvents').textContent='等待旋钮滚动…';$('inputCheck').focus();};
$('connect').onclick=()=>start('status');
$('resume').onclick=()=>start('resume');
$('flash').onclick=async()=>{const f=$('uf2').files[0];if(!f)return start('flash');try{if(f.size>32*1024*1024)throw Error('固件不能大于 32MB');const bytes=new Uint8Array(await f.arrayBuffer());let text='';for(let i=0;i<bytes.length;i+=8192)text+=String.fromCharCode(...bytes.subarray(i,i+8192));start('flash',{uf2:btoa(text)});}catch(e){notice(e.message,true);}};
function updateFirmwareChoice(){
 const f=$('uf2').files[0];
 $('flash').textContent=f?'刷入所选 UF2 固件':'刷入中文 V1 固件';
 $('firmwareKind').textContent=f?'本地自选固件':'内置固件 V1-CN-1.8';
 $('uf2Name').textContent=f?`已选择：${f.name} · ${(f.size/1024).toFixed(1)} KB`:'当前：Hapticpad_V1_CN.uf2（内置）';
}
$('chooseFirmware').onclick=()=>$('uf2').click();
$('useBuiltin').onclick=()=>{$('uf2').value='';updateFirmwareChoice();notice('已切换为内置中文 V1 固件，尚未刷入');};
$('uf2').onchange=()=>{
 const f=$('uf2').files[0];
 if(f&&(!/\.uf2$/i.test(f.name)||!f.size||f.size%512||f.size>32*1024*1024)){
  $('uf2').value='';updateFirmwareChoice();notice('请选择有效的 .uf2 文件（512 字节整块，最大 32MB），已恢复使用内置固件',true);return;
 }
 updateFirmwareChoice();if(f)notice('固件已选好；点击“刷入所选 UF2 固件”开始写入');
};
$('stopMotor').onclick=()=>start('motor',{enabled:false});$('testMotor').onclick=()=>start('motor',{enabled:true});
$('read').onclick=()=>start('read');$('write').onclick=()=>start('write',{config:structuredClone(config)});
$('addSdr').onclick=async()=>{
 try{
  const result=await api('sdr-config',{config:structuredClone(config)});
  try{localStorage.setItem('hapticpad-stable-1.8-before-sdr',JSON.stringify(config));}catch{}
  config=result.config;selected=config.profiles.findIndex(p=>p.name==='SDR通用');changed();render();
  notice('已保留现有配置并追加 / 选中 SDR通用。写入 SD 后，到 Windows 控制台连接 CAT 并开始接管。');
 }catch(e){notice(e.message,true);}
};
function displayTextCount(){const bytes=new TextEncoder().encode($('displayText').value).length;$('displayTextCount').textContent=`${bytes} / 31 UTF-8 字节；屏幕过长文字会裁剪显示`;$('displayText').setCustomValidity(bytes>31?'最多31个UTF-8字节':'');}
$('displayScene').onchange=()=>{active().display_scene=$('displayScene').value;changed();render();};
$('displayText').oninput=()=>{displayTextCount();active().display_text=$('displayText').value;changed();};
$('profileName').onchange=()=>{active().name=$('profileName').value;changed();render();};
$('motorEnabled').onchange=()=>{config.motor_enabled=$('motorEnabled').checked;changed();};
for(const key of ['mode','direction','led','brightness','wheelKey','primary','secondary'])$(key).oninput=()=>{const field=key==='wheelKey'?'wheel_key':key;let v=$(key).value;if(['direction','brightness','wheelKey'].includes(key))v=Number(v);if(['primary','secondary'].includes(key))v=[1,3,5].map(i=>parseInt(v.slice(i,i+2),16));active()[field]=v;if(key==='mode'){if(v!=='Clicky')active().feedback.mode='Follow';modeHelp();renderFeedback();renderFlywheel();}if(key==='brightness')$('brightnessValue').value=v;if(key==='led')ledHelp();changed();};
$('addProfile').onclick=()=>{if(config.profiles.length>=32)return notice('最多 32 套配置',true);const p=structuredClone(active());p.name='新配置';config.profiles.push(p);selected=config.profiles.length-1;changed();render();};
$('deleteProfile').onclick=()=>{if(config.profiles.length<2)return;config.profiles.splice(selected,1);selected=Math.min(selected,config.profiles.length-1);changed();render();};
$('defaults').onclick=()=>{config=structuredClone(defaults);selected=0;changed();render();notice('已恢复编辑器预设，尚未写入设备');};
$('loadTemplate').onclick=()=>{
 const template=configTemplates[$('configTemplate').value];if(!template)return;
 try{localStorage.setItem('hapticpad-stable-1.8-before-template',JSON.stringify(config));}catch{}
 config=structuredClone(template.config);selected=0;changed();render();
 notice('已加载'+template.label+'，电机关闭；点击“写入 SD 并应用”生效');
};
$('importBtn').onclick=()=>$('xmlFile').click();$('xmlFile').onchange=async()=>{try{const f=$('xmlFile').files[0];if(!f)return;if(f.size>65536)throw Error('XML 不能超过 64KB');const result=await api('import',{xml:await f.text()});config=result.config;selected=0;changed();render();notice('配置已导入编辑器，尚未写入设备');}catch(e){notice(e.message,true);}finally{$('xmlFile').value='';}};
$('exportBtn').onclick=async()=>{try{const r=await api('export',{config});download('config.xml',r.xml);notice('config.xml 已导出');}catch(e){notice(e.message,true);}};
(async()=>{try{const r=await(await fetch('/api/bootstrap')).json();token=r.token;feedbackDefaults=r.feedback_defaults;feedbackPresets=r.feedback_presets;flywheelDefaults=r.flywheel_defaults;flywheelPresets=r.flywheel_presets;previewOnly=r.preview_only===true;nativeUsb=r.native_usb===true;setBusy(false);configTemplates=r.config_templates||{};$('configTemplate').replaceChildren(...Object.entries(configTemplates).map(([key,t])=>new Option(t.label,key)));defaults=structuredClone(r.config);config=r.config;try{const draft=JSON.parse(localStorage.getItem('hapticpad-stable-1.8-draft'));if(draft){const checked=await api('export',{config:draft});if(checked.xml){config=draft;dirty=true;$('editState').textContent='已恢复本机草稿，未自动写入设备。';}}}catch{}selected=Math.max(0,config.profiles.findIndex(p=>p.mode==='Flywheel'));render();}catch(e){notice('工具箱未启动，请双击“启动工具箱.command”',true);}})();
