"""Large background uploads are optimized before preview and browser storage."""
from pathlib import Path
import subprocess
import unittest


class BackgroundImageTests(unittest.TestCase):
    def test_small_large_empty_mime_and_failed_image_uploads(self):
        script=r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
let dimensions={width:640,height:360},encoded='data:image/webp;base64,aGVsbG8=',original='data:image/png;base64,aGVsbG8=',decodeFails=false,draws=0;
const context={window:{},FileReader:class{readAsDataURL(){this.result=original;this.onload();}},Image:class{get naturalWidth(){return dimensions.width;}get naturalHeight(){return dimensions.height;}async decode(){if(decodeFails)throw Error('bad image');}},
 document:{createElement(){return {getContext(){return {drawImage(){draws++;}};},toDataURL(){return encoded;}};}}};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);
const api=context.window.SlurmyBackgroundImage;
(async()=>{
 const file={size:100,type:'image/png',name:'background.png'};
 let result=await api.prepare(file);assert.equal(result.image,original);assert.equal(result.optimized,false);assert.equal(draws,0);
 result=await api.prepare({...file,type:''});assert.equal(result.image,original);
 dimensions={width:6000,height:4000};original='data:image/png;base64,'+'a'.repeat(3000000);
 result=await api.prepare({...file,size:4*1024*1024});assert.equal(result.image,encoded);assert.equal(result.optimized,true);assert.ok(draws>0);
 await assert.rejects(api.prepare({...file,size:21*1024*1024}),/20 MB/);
 await assert.rejects(api.prepare({...file,type:'image/svg+xml'}),/PNG/);
 decodeFails=true;await assert.rejects(api.prepare(file),/could not open/);decodeFails=false;
 encoded='data:image/webp;base64,'+'a'.repeat(800000);await assert.rejects(api.prepare({...file,size:4*1024*1024}),/browser storage/);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        path=Path(__file__).parent/'slurmy_web/static/background-image.js'
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
