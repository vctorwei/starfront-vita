// MIT, Copyright (c) 2026 vctorwei. Original geometric loader shell art.
// npm install --no-save sharp@0.34.5; node scripts/make_shell_art.cjs
const fs = require('fs');
const path = require('path');
const sharp = require(process.env.SF_SHARP_MODULE || 'sharp');
const root = path.resolve(__dirname, '..');
const mark = '<path d="M78 34H46L34 46V58L46 70H66V82H34V94H68L80 82V68L68 56H48V46H78Z M92 34H126V46H106V58H124V70H106V94H92Z" fill="#c9faff"/>';
function svg(w,h,icon=false) {
 const k=icon?.8:2;
 return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><rect width="${w}" height="${h}" fill="#101d33"/><path d="M0 ${h*.75}L${w} ${h*.25}M0 ${h*.25}L${w} ${h*.75}" stroke="#1c3854" stroke-width="2"/><rect x="4" y="4" width="${w-8}" height="${h-8}" rx="${icon?20:12}" fill="none" stroke="#52cad6" stroke-width="3"/><g transform="translate(${(w-160*k)/2},${(h-128*k)/2}) scale(${k})">${mark}</g></svg>`;
}
(async()=>{
 for(const [name,w,h,icon] of [['icon0.png',128,128,true],['pic0.png',960,544,false],['livearea/contents/cover-bg06.png',840,500,false],['livearea/contents/cover-gate06.png',280,158,true]]) {
  const target=path.join(root,'assets/sce_sys',name);fs.mkdirSync(path.dirname(target),{recursive:true});
  const source=svg(w,h,icon);fs.writeFileSync(target.replace(/\.png$/,'.svg'),source+'\n');
  await sharp(Buffer.from(source)).png({palette:true,colours:128,dither:0}).toFile(target);
 }
})();
