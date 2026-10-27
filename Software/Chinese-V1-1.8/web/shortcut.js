'use strict';
// Convert a physical browser key chord into the VK actions understood by V1.
const ShortcutCapture = (()=>{
 const special={Backspace:8,Tab:9,Enter:13,NumpadEnter:13,Pause:19,CapsLock:20,Escape:27,Space:32,PageUp:33,PageDown:34,End:35,Home:36,ArrowLeft:37,ArrowUp:38,ArrowRight:39,ArrowDown:40,Insert:45,Delete:46,NumpadAdd:107,NumpadSubtract:109,NumpadDecimal:110,NumpadDivide:111,NumLock:144,ScrollLock:145,Semicolon:186,Equal:187,Comma:188,Minus:189,Period:190,Slash:191,Backquote:192,BracketLeft:219,Backslash:220,BracketRight:221,Quote:222};
 function fromEvent(event){
  const codes=[];
  if(event.metaKey)codes.push(91);
  if(event.ctrlKey)codes.push(17);
  if(event.altKey)codes.push(18);
  if(event.shiftKey)codes.push(16);
  if(/^(Control|Shift|Alt|Meta)(Left|Right)$/.test(event.code)||['Control','Shift','Alt','Meta','AltGraph'].includes(event.key))return {codes,complete:false};
  let main=special[event.code];
  if(/^Key[A-Z]$/.test(event.code))main=event.code.charCodeAt(3);
  else if(/^Digit[0-9]$/.test(event.code))main=event.code.charCodeAt(5);
  else if(/^Numpad[0-9]$/.test(event.code))main=96+Number(event.code.slice(6));
  else if(/^F([1-9]|1[0-2])$/.test(event.code))main=111+Number(event.code.slice(1));
  if(!main)throw Error('暂不支持这个按键，请使用快捷操作列表或高级设置。');
  codes.push(main);
  if(codes.length>3)throw Error('当前固件最多支持两个修饰键加一个普通键，请减少修饰键。');
  return {codes,complete:true};
 }
 function guessLabel(codes){
  const joined=codes.join(',');
  if(['91,16,90','17,16,90','17,89'].includes(joined))return '重做';
  if(codes.length===2&&[17,91].includes(codes[0]))return ({65:'全选',67:'复制',70:'查找',78:'新建',79:'打开',80:'打印',83:'保存',86:'粘贴',87:'关闭',88:'剪切',90:'撤销'})[codes[1]]||'自定义';
  if(codes.length===1)return ({8:'退格',9:'Tab',13:'回车',27:'取消',32:'空格',37:'左箭头',38:'上箭头',39:'右箭头',40:'下箭头',46:'删除'})[codes[0]]||'自定义';
  return '自定义';
 }
 return {fromEvent,guessLabel};
})();
if(typeof module!=='undefined')module.exports=ShortcutCapture;
