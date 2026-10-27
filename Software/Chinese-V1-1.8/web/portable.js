/* Browser-only portable transport. No automatic device writes. */
let portableRunning=false;
const portableIds=['browserInfo','browserSd','browserBoot','browserResume','folderReadConfig','folderConfig','folderFirmware'];
async function portableRun(task){
 if(portableRunning||!$('activity').hidden)return;
 portableRunning=true;setBusy(true);portableIds.forEach(id=>$(id).disabled=true);
 try{$('portableStatus').textContent='处理中…';await task();}
 catch(e){$('portableStatus').textContent=e.name==='AbortError'?'已取消。':e.message;}
 finally{portableRunning=false;setBusy(false);portableIds.forEach(id=>$(id).disabled=false);}
}
function needDirectories(){if(!window.showDirectoryPicker)throw Error('此浏览器不支持目录写入。请使用Chrome/Edge，或导出config.xml后手动复制。');}
async function writeFile(dir,name,data){const handle=await dir.getFileHandle(name,{create:true});const stream=await handle.createWritable();try{await stream.write(data);await stream.close();}catch(e){try{await stream.abort();}catch{}throw e;}return handle;}
async function readConfig(dir){const handle=await dir.getFileHandle('config.xml');const xml=await(await handle.getFile()).text();const checked=await api('import',{xml});return{xml,config:checked.config};}
async function serialRequest(command){
 if(!navigator.serial)throw Error('此浏览器不支持Web Serial，请使用Chrome/Edge。');
 const port=await navigator.serial.requestPort({filters:[{usbVendorId:0x239a,usbProductId:0xcafe}]});
 let reader,writer,timer;
 try{
  await port.open({baudRate:115200});await port.setSignals({dataTerminalReady:true});await new Promise(resolve=>setTimeout(resolve,120));writer=port.writable.getWriter();reader=port.readable.getReader();
  let pending='';const decoder=new TextDecoder();
  async function exchange(cmd){
   await writer.write(new TextEncoder().encode(cmd+'\n'));
   const deadline=Date.now()+5000;
   while(Date.now()<deadline){
    const read=reader.read();
    const result=await Promise.race([read,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('串口响应超时，请关闭其他串口程序后重试。')),Math.max(1,deadline-Date.now()));})]);clearTimeout(timer);
    if(result.done)throw Error('设备已断开；请重新连接并检测状态。');
    pending+=decoder.decode(result.value,{stream:true});
    let end;while((end=pending.indexOf('\n'))>=0){const line=pending.slice(0,end).trim();pending=pending.slice(end+1);if(line.startsWith(cmd==='HP INFO'?'HP INFO ':'HP OK '))return line;}
   }
   throw Error('设备未返回预期响应。');
  }
  const infoLine=await exchange('HP INFO');const info=JSON.parse(infoLine.slice(8));
  if(!String(info.version||'').includes('CN'))throw Error('所选设备没有报告中文固件，未发送控制命令。');
  if(command==='HP INFO'){$('portableStatus').textContent=`已连接：${info.version}，SD${info.sd?'正常':'未就绪'}。`;return;}
  // Mode-switching commands intentionally disconnect USB. Only report submission, not completion.
  await writer.write(new TextEncoder().encode(command+'\n'));
  $('portableStatus').textContent=command==='HP SDMODE'?'已发送SD卡模式请求，等待系统显示U盘，再选择SD目录。':command==='HP BOOT'?'已发送刷机模式请求，等待出现RPI-RP2盘。':'已发送恢复请求，请检查屏幕和按键。';
 }finally{clearTimeout(timer);if(reader){try{await reader.cancel();}catch{}try{reader.releaseLock();}catch{}}if(writer){try{writer.releaseLock();}catch{}}try{await port.close();}catch{}}
}
$('browserInfo').onclick=()=>portableRun(()=>serialRequest('HP INFO'));
$('browserSd').onclick=()=>portableRun(()=>serialRequest('HP SDMODE'));
$('browserBoot').onclick=()=>portableRun(()=>serialRequest('HP BOOT'));
$('browserResume').onclick=()=>portableRun(()=>serialRequest('HP REBOOT'));
$('folderReadConfig').onclick=()=>portableRun(async()=>{needDirectories();const dir=await window.showDirectoryPicker({mode:'read'});const old=await readConfig(dir);config=old.config;selected=0;dirty=false;render();savedDraft();$('portableStatus').textContent='已读取SD卡config.xml，可开始编辑。';});
$('folderConfig').onclick=()=>portableRun(async()=>{
 needDirectories();const dir=await window.showDirectoryPicker({mode:'readwrite'});
 // Existing parseable config is required, so an arbitrary folder cannot be overwritten.
 const old=await readConfig(dir);const exported=await api('export',{config});
 const backup='config-backup-'+new Date().toISOString().replace(/[:.]/g,'-')+'.xml';
 const backupHandle=await writeFile(dir,backup,old.xml);
 if(await(await backupHandle.getFile()).text()!==old.xml)throw Error('备份校验失败，未覆盖config.xml。');
 const handle=await writeFile(dir,'config.xml',exported.xml);
 if(await(await handle.getFile()).text()!==exported.xml)throw Error('写入后校验失败。请保留U盘模式并检查备份。');
 dirty=false;savedDraft();$('editState').textContent='配置已写入SD，等待安全弹出并恢复设备。';
 $('portableStatus').textContent=`写入并读回校验通过，旧文件备份为${backup}。请在系统中安全弹出SD卡，再点击“安全弹出SD后恢复使用”。`;
});
$('folderFirmware').onclick=()=>portableRun(async()=>{
 needDirectories();const dir=await window.showDirectoryPicker({mode:'readwrite'});
 const marker=await(await(await dir.getFileHandle('INFO_UF2.TXT')).getFile()).text();
 if(!/RPI-RP2|RP2040/i.test(marker))throw Error('所选目录不是RP2040刷机盘。');
 const file=$('uf2').files[0];const bytes=new Uint8Array(file?await file.arrayBuffer():await(await fetch('/firmware.uf2')).arrayBuffer());
 let raw='';for(let i=0;i<bytes.length;i+=8192)raw+=String.fromCharCode(...bytes.subarray(i,i+8192));
 const verified=await api('validate-firmware',{uf2:btoa(raw)});
 try{await writeFile(dir,'Hapticpad.uf2',bytes);}catch(e){throw Error('复制未能确认完成（刷机盘也可能已自动断开）。请重新连接检测固件版本后判断，勿直接视为刷入成功。'+e.message);}
 $('portableStatus').textContent='UF2结构校验通过，复制已提交。此流程没有Flash读回校验；请等待设备重启，重新连接检测版本。SHA256：'+verified.sha256;
});
