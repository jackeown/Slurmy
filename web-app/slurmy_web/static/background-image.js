'use strict';

// Keep large wallpaper files out of localStorage while retaining a sharp preview.
window.SlurmyBackgroundImage = {
  async prepare(file) {
    if(file.size > 20*1024*1024) throw new Error('Choose an image smaller than 20 MB.');
    const supported = /^image\/(png|jpeg|webp|gif|avif|bmp|x-ms-bmp)$/i.test(file.type || '') ||
      (!file.type && /\.(png|jpe?g|webp|gif|avif|bmp)$/i.test(file.name || ''));
    if(!supported) throw new Error('Choose a PNG, JPEG, WebP, GIF, AVIF, or BMP image.');
    const original = await new Promise((resolve,reject) => {
      const reader = new FileReader();reader.onload=()=>resolve(reader.result);
      reader.onerror=()=>reject(new Error('Could not read this image. Try choosing it again.'));
      reader.readAsDataURL(file);
    });
    const image = new Image();image.src=original;
    try { await image.decode(); } catch { throw new Error('This browser could not open the image. Try a PNG, JPEG, or WebP copy.'); }
    const width=image.naturalWidth,height=image.naturalHeight;
    if(!width || !height)throw new Error('This image has no readable dimensions.');
    const budget=700000;
    if(original.length<=budget && Math.max(width,height)<=2560 && /^data:image\/(png|jpeg|webp|gif);base64,/i.test(original))
      return {image:original,imageName:file.name||'Background image',optimized:false};
    const canvas=document.createElement('canvas');
    let scale=Math.min(1,2560/Math.max(width,height));
    for(let pass=0;pass<6;pass++) {
      canvas.width=Math.max(1,Math.round(width*scale));canvas.height=Math.max(1,Math.round(height*scale));
      const ctx=canvas.getContext('2d');
      if(!ctx)throw new Error('Image optimization is unavailable in this browser.');
      ctx.drawImage(image,0,0,canvas.width,canvas.height);
      for(const quality of [.84,.7,.55]) {
        const encoded=canvas.toDataURL('image/webp',quality);
        if(encoded.length<=budget && /^data:image\/(webp|png|jpeg);base64,/i.test(encoded))
          return {image:encoded,imageName:file.name||'Background image',optimized:true};
      }
      scale*=.7;
    }
    throw new Error('Could not fit this image into browser storage. Try a smaller image.');
  },
};
