'use strict';
const $=id=>document.getElementById(id);
const ui={title:$('title'),titleBg:$('titleBg'),help:$('help'),game:$('game'),bg:$('background'),scene:$('scene'),walker:$('walker'),sparks:$('sparkles'),tap:$('tapMark'),fade:$('fade'),obj:$('objective'),progress:$('progress'),hint:$('contextHint'),dialog:$('dialog'),speaker:$('speaker'),text:$('dialogText'),inv:$('inventory'),docs:$('docs'),docText:$('docText'),ending:$('ending')};
const SAVE='tenebris-carmona-v4';
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
const distance=(a,b)=>Math.hypot(a[0]-b[0],a[1]-b[1]);
const nav={
 nodes:{spawn:[25,88],left:[34,82],center:[47,82],right:[68,84],mid:[50,73],upper:[55,66],door:[63,61],trace:[47,69]},
 edges:{spawn:['left'],left:['spawn','center','trace'],center:['left','right','mid'],right:['center'],mid:['center','trace','upper'],trace:['left','mid','upper'],upper:['mid','trace','door'],door:['upper']}
};
const hotspots=[
 {id:'report',x:26,y:77,dest:'left',title:'Note de périmètre',doc:'Henderson signale que le périmètre de Carmona a été levé trop vite. Un témoin mentionne Morrow, chambre 3.',speaker:'WALKER',text:'Le périmètre a été nettoyé avant notre arrivée. Et quelqu’un a laissé Morrow dans la marge.'},
 {id:'trace',x:49,y:61,dest:'trace',title:'Résidu noir',doc:'Dépôt organique noir dans une fissure du bitume. Aucune odeur. Réagit faiblement à la chaleur de la lampe.',speaker:'WALKER',text:'Ce n’est pas de l’huile. Et ce truc réagit à la chaleur…'},
 {id:'door',x:63.3,y:47.5,dest:'door',title:'Accès Morrow',doc:'Porte de service reliée aux anciens volumes techniques de Morrow. Serrure récemment manipulée.',speaker:'WALKER',text:'Porte forcée récemment. Chambre 3 est de l’autre côté. Voilà la suite.'}
];
let S={pos:[25,88],route:[],seen:{},docs:{},dialog:[],inDialog:false,playing:false,selected:null,action:null,lastNode:'spawn'};
function storageGet(){try{return localStorage.getItem(SAVE)}catch(e){return null}}function storageSet(v){try{localStorage.setItem(SAVE,v)}catch(e){}}
function save(){if(!S.playing)return;storageSet(JSON.stringify({pos:S.pos,seen:S.seen,docs:S.docs,lastNode:S.lastNode}))}
function restore(){try{const d=JSON.parse(storageGet());if(!d)return false;S.pos=d.pos||[25,88];S.seen=d.seen||{};S.docs=d.docs||{};S.lastNode=d.lastNode||'spawn';return true}catch(e){return false}}
async function loadBackground(){
 try{
  const parts=await Promise.all([0,1,2].map(i=>fetch(`assets/v4/alley.${i}.txt?v=4`).then(r=>{if(!r.ok)throw Error(r.status);return r.text()})));
  const b64=parts.join('').trim();if(!b64.startsWith('UklG')||b64.length<30000)throw Error('asset incomplet');
  const url='data:image/webp;base64,'+b64;ui.bg.src=url;ui.titleBg.style.backgroundImage=`linear-gradient(rgba(0,0,0,.42),rgba(0,0,0,.7)),url("${url}")`;return;
 }catch(e){ui.bg.src='assets/alley_ps1.jpg?v=4';ui.titleBg.style.backgroundImage='linear-gradient(rgba(0,0,0,.42),rgba(0,0,0,.7)),url("assets/alley_ps1.jpg?v=4")'}
}
function show(screen){document.querySelectorAll('.screen').forEach(x=>x.classList.remove('visible'));if(screen)screen.classList.add('visible')}
function safeTap(el,fn){let t=0;const go=e=>{e.preventDefault();e.stopPropagation();const n=Date.now();if(n-t<260)return;t=n;fn(e)};el.addEventListener('click',go);el.addEventListener('touchend',go,{passive:false})}
function nearestNode(p){let best='spawn',bd=1e9;for(const [k,n] of Object.entries(nav.nodes)){const d=distance(p,n);if(d<bd){bd=d;best=k}}return best}
function bfs(a,b){if(a===b)return[a];const q=[[a]],seen=new Set([a]);while(q.length){const path=q.shift(),n=path[path.length-1];for(const nx of nav.edges[n]||[]){if(seen.has(nx))continue;const p=[...path,nx];if(nx===b)return p;seen.add(nx);q.push(p)}}return[a]}
function nearestGraphPoint(p){let best=null,bd=1e9,anchor='spawn',seen=new Set();for(const [a,list] of Object.entries(nav.edges)){for(const b of list){const key=[a,b].sort().join('|');if(seen.has(key))continue;seen.add(key);const A=nav.nodes[a],B=nav.nodes[b],vx=B[0]-A[0],vy=B[1]-A[1],l=vx*vx+vy*vy;const t=l?clamp(((p[0]-A[0])*vx+(p[1]-A[1])*vy)/l,0,1):0;const q=[A[0]+vx*t,A[1]+vy*t],d=distance(p,q);if(d<bd){bd=d;best=q;anchor=distance(q,A)<=distance(q,B)?a:b}}}return{point:best||nav.nodes.spawn,anchor}}
function routeToPoint(point,anchor,action=null){const start=nearestNode(S.pos),keys=bfs(start,anchor);S.route=keys.slice(1).map(k=>nav.nodes[k].slice());S.route.push(point.slice());S.action=action}
function routeToNode(name,action=null){const start=nearestNode(S.pos),keys=bfs(start,name);S.route=keys.slice(1).map(k=>nav.nodes[k].slice());if(!S.route.length)S.route=[nav.nodes[name].slice()];S.action=action}
function screenPoint(e){const r=ui.scene.getBoundingClientRect(),cx=e.clientX,cy=e.clientY;return[(cx-r.left)/r.width*100,(cy-r.top)/r.height*100]}
function mark(p){ui.tap.style.left=p[0]+'%';ui.tap.style.top=p[1]+'%';ui.tap.classList.remove('show');void ui.tap.offsetWidth;ui.tap.classList.add('show')}
function tapGround(e){if(!S.playing||S.inDialog||!ui.inv.classList.contains('hidden'))return;const p=screenPoint(e);mark(p);const proj=nearestGraphPoint(p);routeToPoint(proj.point,proj.anchor)}
function drawWalker(){const [x,y]=S.pos;ui.walker.style.left=x+'%';ui.walker.style.top=y+'%';const scale=clamp(.72+(y-60)*.018,.62,1.28);ui.walker.style.transform=`translate(-50%,-92%) scale(${scale})`}
function createSparks(){ui.sparks.innerHTML='';for(const h of hotspots){if(S.seen[h.id])continue;const d=document.createElement('div');d.className='spark';d.style.left=h.x+'%';d.style.top=h.y+'%';d.innerHTML='<i></i>';safeTap(d,()=>{if(S.inDialog)return;routeToNode(h.dest,()=>inspect(h))});ui.sparks.appendChild(d);pulse(d,Math.random()*900)}updateHUD()}
function pulse(el,delay=0){setTimeout(function again(){if(!el.isConnected)return;el.classList.remove('glint');void el.offsetWidth;el.classList.add('glint');setTimeout(again,1450+Math.random()*1800)},delay)}
function inspect(h){if(S.seen[h.id])return;S.seen[h.id]=1;S.docs[h.title]=h.doc;createSparks();say([[h.speaker,h.text]]);save();if(Object.keys(S.seen).length===3)setTimeout(()=>say([['MERCER','Walker. Tu as assez vu. Reviens au QG.'],['WALKER','Négatif. La porte mène à Morrow. Quelqu’un a construit cette piste pour moi.'],['WALKER','Je vais voir jusqu’où elle va.']]),100)}
function say(lines){S.dialog.push(...lines);if(!S.inDialog)nextDialog()}
function nextDialog(){const d=S.dialog.shift();if(!d){S.inDialog=false;ui.dialog.classList.add('hidden');if(Object.keys(S.seen).length===3){ui.obj.textContent='Rejoindre Morrow — chambre 3.';ui.progress.textContent='Accès identifié';setTimeout(finish,600)}return}S.inDialog=true;ui.speaker.textContent=d[0];ui.text.textContent=d[1];ui.dialog.classList.remove('hidden')}
function updateHUD(){const n=Object.keys(S.seen).length;ui.progress.textContent=`${n} / 3 indices`;ui.obj.textContent=n<3?'Inspecter les indices qui scintillent.':'Rejoindre Morrow — chambre 3.'}
function finish(){if(!S.playing)return;S.playing=false;ui.fade.classList.add('on');setTimeout(()=>{ui.game.classList.add('hidden');ui.fade.classList.remove('on');show(ui.ending)},400)}
function start(cont=false){S={pos:[25,88],route:[],seen:{},docs:{},dialog:[],inDialog:false,playing:true,selected:null,action:null,lastNode:'spawn'};if(cont)restore();show(null);ui.game.classList.remove('hidden');ui.inv.classList.add('hidden');createSparks();drawWalker();if(!cont)say([['MERCER','Walker. Carmona d’abord. Rien ne justifie de descendre plus bas.'],['WALKER','Alors pourquoi quelqu’un a pris le temps de nettoyer la scène ?']]);save()}
function renderDocs(){ui.docs.innerHTML='';const es=Object.entries(S.docs);if(!es.length){ui.docs.innerHTML='<div class="doc">AUCUN DOCUMENT</div>';ui.docText.textContent='Les indices inspectés apparaîtront ici.';return}for(const [t,c] of es){const b=document.createElement('button');b.className='doc'+(S.selected===t?' sel':'');b.textContent=t;safeTap(b,()=>{S.selected=t;ui.docText.textContent=c;renderDocs()});ui.docs.appendChild(b)}}
let last=performance.now();function loop(now){const dt=Math.min(.035,(now-last)/1000);last=now;if(S.playing&&!S.inDialog&&S.route.length){const target=S.route[0],dx=target[0]-S.pos[0],dy=target[1]-S.pos[1],d=Math.hypot(dx,dy),sp=19;if(Math.abs(dx)>.04)ui.walker.classList.toggle('facingLeft',dx<0);ui.walker.classList.add('walking');if(d<.35){S.pos=target;S.route.shift();if(!S.route.length){ui.walker.classList.remove('walking');S.lastNode=nearestNode(S.pos);const a=S.action;S.action=null;if(a)a();save()}}else{S.pos[0]+=dx/d*sp*dt;S.pos[1]+=dy/d*sp*dt}drawWalker()}else ui.walker.classList.remove('walking');requestAnimationFrame(loop)}requestAnimationFrame(loop);
ui.scene.addEventListener('pointerup',tapGround);safeTap(ui.dialog,nextDialog);safeTap($('newBtn'),()=>start(false));safeTap($('continueBtn'),()=>start(true));safeTap($('helpBtn'),()=>show(ui.help));safeTap($('backBtn'),()=>show(ui.title));safeTap($('invBtn'),()=>{renderDocs();ui.inv.classList.remove('hidden')});safeTap($('closeInv'),()=>ui.inv.classList.add('hidden'));safeTap($('againBtn'),()=>{try{localStorage.removeItem(SAVE)}catch(e){}show(ui.title);ui.game.classList.add('hidden')});$('continueBtn').disabled=!storageGet();window.addEventListener('pagehide',save);loadBackground();show(ui.title);drawWalker();