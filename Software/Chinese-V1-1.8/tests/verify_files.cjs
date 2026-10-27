const fs=require('fs'),vm=require('vm'),assert=require('assert');
const elements=new Map();const el=id=>{if(!elements.has(id))elements.set(id,{hidden:true,files:[],textContent:''});return elements.get(id);};
let operations=[],failure=false,dirty=true,config={profiles:[]};
function dir(initial){const files=new Map(Object.entries(initial));return{files,async getFileHandle(name,options={}){if(!files.has(name)&&!options.create)throw Error('not found');return{async getFile(){return{text:async()=>files.get(name)};},async createWritable(){return{async write(data){operations.push(name);files.set(name,data);},async close(){},async abort(){}};}};}};}
let target;const context={console,TextEncoder,TextDecoder,setTimeout,clearTimeout,navigator:{},window:{showDirectoryPicker:async()=>target},$:el,setBusy:()=>{},api:async(action,p)=>{if(action==='import'){if(p.xml!=='old')throw Error('invalid');return{config:{}};}if(action==='export')return{xml:'new'};},config,dirty,savedDraft:()=>{},render:()=>{},Date};
vm.createContext(context);vm.runInContext(fs.readFileSync('web/portable.js','utf8'),context);
(async()=>{
 target=dir({});await el('folderConfig').onclick();assert.equal(operations.length,0,'arbitrary folder must not be modified');
 target=dir({'config.xml':'invalid'});await el('folderConfig').onclick();assert.equal(operations.length,0,'invalid XML must not be overwritten');
 target=dir({'config.xml':'old'});await el('folderConfig').onclick();assert.equal(operations.length,2);assert(operations[0].startsWith('config-backup-'));assert.equal(operations[1],'config.xml');assert.equal(target.files.get('config.xml'),'new');assert.equal(target.files.get(operations[0]),'old');assert(el('portableStatus').textContent.includes('读回校验通过'));
 operations=[];target=dir({'INFO_UF2.TXT':'unrelated disk'});await el('folderFirmware').onclick();assert.equal(operations.length,0,'non-RP2040 disk must not be modified');
 console.log('4 file-safety checks passed: missing config, invalid config, backup/write/readback, wrong boot disk.');
})().catch(e=>{console.error(e);process.exitCode=1;});
