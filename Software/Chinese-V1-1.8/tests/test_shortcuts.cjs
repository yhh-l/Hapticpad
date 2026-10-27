const test=require('node:test');
const assert=require('node:assert/strict');
const {fromEvent,guessLabel}=require('../web/shortcut.js');
const chord=(code,flags={})=>fromEvent({code,key:code,...flags});
test('Mac copy and redo preserve GUI and Shift before the letter',()=>{
 assert.deepEqual(chord('KeyC',{metaKey:true}).codes,[91,67]);
 assert.deepEqual(chord('KeyZ',{metaKey:true,shiftKey:true}).codes,[91,16,90]);
 assert.equal(guessLabel([91,67]),'复制');assert.equal(guessLabel([91,16,90]),'重做');
});
test('Windows control shortcuts and Delete use distinct VK actions',()=>{
 assert.deepEqual(chord('KeyV',{ctrlKey:true}).codes,[17,86]);
 assert.deepEqual(chord('Delete').codes,[46]);assert.deepEqual(chord('Backspace').codes,[8]);
 assert.equal(guessLabel([17,86]),'粘贴');assert.equal(guessLabel([46]),'删除');
});
test('Modifier-only press waits; overlong chords are rejected without truncation',()=>{
 assert.deepEqual(fromEvent({code:'ControlLeft',key:'Control',ctrlKey:true}),{codes:[17],complete:false});
 assert.throws(()=>chord('KeyC',{ctrlKey:true,altKey:true,shiftKey:true}),/最多支持/);
});
test('Physical codes work even with Chinese IME Process key text',()=>{
 assert.deepEqual(fromEvent({code:'KeyC',key:'Process',ctrlKey:true}).codes,[17,67]);
});
test('Escape, punctuation, digits and function keys match V1 VK codes',()=>{
 for(const [code,vk] of [['Escape',27],['Semicolon',186],['BracketLeft',219],['Digit7',55],['Numpad7',103],['F12',123]])assert.deepEqual(chord(code).codes,[vk]);
});
test('Unsupported system/media keys fail visibly instead of saving empty actions',()=>{
 assert.throws(()=>chord('Unidentified'),/暂不支持/);assert.throws(()=>chord('AudioVolumeUp'),/暂不支持/);assert.throws(()=>chord('F13'),/暂不支持/);
});
