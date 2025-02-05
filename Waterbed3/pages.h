const char index_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WiFi Waterbed Heater</title>
<style type="text/css">
table{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(94,94,94,1) 0%, rgba(160,160,160,1) 90%);
background-clip: padding-box;
}
input{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(160,160,160,1) 0%, rgba(239,255,255,1) 100%);
background-clip: padding-box;
}
div{
border-radius: 2px;
box-shadow: 2px 2px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(200,190,200,1) 0%, rgba(120,120,120,1) 100%);
background-clip: padding-box;
}

body{width:300px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.4/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript">
a=document.all
oledon=0
avg=1
cnt=1
eco=0
debug=false
cf='F'
pause=0
zoom=1
min2=0
max2=300
dispIdx=0
mod=5
btn=0
va=new Array()

//   t,   pres,   mov , sta,   inBed,  distS distM   en   enM
colors=['#00F','#0F0','#FF0','#0FF','#F0F','#F00','#77F','#777']

function openSocket(){
ws=new WebSocket("ws://"+window.location.host+"/ws")
//ws=new WebSocket("ws://192.168.31.8/ws")

ws.onopen=function(evt){}
ws.onclose=function(evt){alert("Connection closed.");}
ws.onmessage=function(evt){
 console.log(evt.data)
 d=JSON.parse(evt.data)
 if(d.cmd=='state')
 {
  dt=new Date(d.t*1000)
  a.time.innerHTML=dt.toLocaleTimeString()
  waterTemp=+d.waterTemp
  a.temp.innerHTML=(d.on?"<font color='red'><b>":"")+waterTemp+"&deg"+cf+(d.on?"</b></font>":"")
  a.tgt.innerHTML="> "+d.hiTemp+"&deg"+cf
  a.rt.innerHTML=+d.temp+'&deg'+cf
  a.rh.innerHTML=+d.rh+'%'
  eta=+d.eta
//  if(eta==0) eta=+d.cooleta
  a.eta.innerHTML='ETA '+t2hms(eta)
 }
 else if(d.cmd=='set')
 {
  a.vt.value=+d.vt
  oledon=d.o
  a.OLED.value=oledon?'ON ':'OFF'
  avg=d.avg
  a.AVG.value=avg?'ON ':'OFF'
  ppkw=d.ppkwh/1000
  a.K.value=ppkw
  a.vo.value=d.vo?'ON ':'OFF'
  a.eco.value=d.e?'ON ':'OFF'
  eco=d.e
  watts=d.w
  idx=d.idx
  schedules=d.item
  schedCnt=d.cnt
  setSeason(d.season)
  draw_bars(d.ts,d.ppkwm)
  a.free.innerHTML='Internal: '+d.freek+' KB Free'
  sdfree = +d.freekSD
  if(sdfree > 1048576)
    a.freeSD.innerHTML='SDCard: '+(+d.freekSD/1048576).toFixed(2)+' GB Free'
  else if(sdfree > 10000)
    a.freeSD.innerHTML='SDCard: '+(+d.freekSD/1024).toFixed()+' MB Free'
  else
    a.freeSD.innerHTML='SDCard: '+d.freekSD+' KB Free'
 }
 else if(d.cmd=='tdata')
 {
  tdata=d.temp
  draw()
 }
 else if(d.cmd=='radar')
 {
  if(min2>+d.dist) min2=+d.dist
  if(min2>+d.distM) min2=+d.distM
  if(max2<+d.dist) max2=+d.dist
  if(max2<+d.distM) max2=+d.distM
   aa=[+d.t,(+d.bits&1)?50:0,(+d.bits>>1&3)*50,(+d.bits&4)?50:0,+d.dist,+d.distM,+d.en,+d.enM]
   va.push(aa)
   draw_chart()
 }
 else if(d.cmd=='files')
 {
   for(i=0;i<d.list.length;i++)
    AddFile(d.list[i])
 }
 else if(d.cmd=='filesSD')
 {
   for(i=0;i<d.list.length;i++)
    AddFileSD(d.list[i])
 }
 else if(d.cmd=='alert')
 {
  alert(d.data)
 }
}
}

function onChangeSched(ent)
{
 id=ent.id.substr(0,1)
 idx=ent.id.substr(1)
 val=ent.value
 if(id=='S')
 {
  tm=val.split(':')
  val=(+tm[0]*60)+(+tm[1])
 }
 setVar('I',idx)
 setVar(id,val)
 draw()
}
function setSeason(s)
{
  seasEdit=s
  for(i=0;i<4;i++)
    document.getElementById('s'+i).setAttribute('style',(i==seasEdit)?'background-color:red':'none')
  itms=schedules[seasEdit]
  cnt=schedCnt[seasEdit]
  for(i=0;i<8;i++){
   item=document.getElementById('r'+i); item.setAttribute('style',i<cnt?'':'display:none')
   if(i==idx) item.setAttribute('style','background-color:red')
   h=Math.floor(itms[i][0]/60)
   m=itms[i][0]%60
   if(m<10) m='0'+m
   document.getElementById('S'+i).value=h+':'+m
   document.getElementById('T'+i).value=itms[i][1]
   document.getElementById('H'+i).value=itms[i][2]
   document.getElementById('S'+i).onchange=function(){onChangeSched(this)}
   document.getElementById('T'+i).onchange=function(){onChangeSched(this)}
   document.getElementById('H'+i).onchange=function(){onChangeSched(this)}
  }
  a.inc.value=Math.min(8,cnt+1)
  a.dec.value=Math.max(1,cnt-1)
}

function AddFile(item)
{
  tr=document.createElement("tr")
  td=document.createElement("td")
  inp=document.createElement("input")
  inp.id=idx
  inp.type='image'
  inp.value=item[0]
  inp.src='del-btn.png'
  inp.onclick=function(){delfile(this.id,this.value)}
  td.appendChild(inp)
  tr.appendChild(td)

  td=document.createElement("td")
  td.style.textAlign = "left";
  td.innerHTML=' &nbsp; '+item[0].substr(0,31)+' '
  tr.appendChild(td)

  td=document.createElement("td")
  td.innerHTML=' '+(item[1]/1024).toFixed()+'K'
  tr.appendChild(td)

  tbody=document.createElement("tbody")
  tbody.id='tbody'+idx
  tbody.appendChild(tr)
  a.fileList.appendChild(tbody)
  idx++
}

function AddFileSD(item)
{
  tr=document.createElement("tr")
  td=document.createElement("td")
  inp=document.createElement("input")
  inp.id=idx
  inp.type='image'
  inp.value=item[0]
  inp.src='del-btn.png'
  inp.onclick=function(){delfileSD(this.id,this.value)}
  td.appendChild(inp)
  tr.appendChild(td)

  td=document.createElement("td")
  td.innerHTML=' '+item[0]+' '
  td.style.textAlign = 'left'
  td.style.overflow = 'hidden'
  td.style.textOverflow = 'ellipsis'
//  td.style.whiteSpace = 'nowrap'
  tr.appendChild(td)

  td=document.createElement("td")
  td.innerHTML=' '+(item[1]/1024).toFixed()+'K'
  tr.appendChild(td)

  tbody=document.createElement("tbody")
  tbody.id='tbody'+idx
  tbody.appendChild(tr)
  a.fileListSD.appendChild(tbody)
  idx++
}

function delfile(idx,name)
{
  setVar('delf', name)
  l=document.getElementById('tbody'+idx)
  a.fileList.removeChild(l)
}

function delfileSD(idx,name)
{
  setVar('delfsd', name)
  l=document.getElementById('tbody'+idx)
  a.fileListSD.removeChild(l)
}

function setVar(varName, value)
{
 ws.send('{"key":"'+a.myKey.value+'","'+varName+'":'+value+'}')
}
function oled(){
setVar('oled',oledon?0:1)
}
function setavg(){avg=!avg
setVar('avg',avg?1:0)
draw()
}
function setTemp(n){
setVar('tadj',n)
}
function setAllTemp(n){
setVar('aadj',n)
}
function setEco(n){
eco=!eco
setVar('eco',eco?1:0)
}
function setVaca(){
setVar('vacatemp',a.vt.value)
setVar('vaca',(a.vo.value=='OFF')?true:false)
}
function setCnt(n){
cnt+=n
setVar('cnt',cnt)
}
function setPPK(){
setVar('ppkwh',(a.K.value*1000).toFixed())
}

var x,y,llh=0,llh2=0
function draw(){
 try{
  c2=document.getElementById('canva')
  ctx=c2.getContext("2d")
  c2h=c2.height-10
  ctx.fillStyle="#88f" // bg
  ctx.fillRect(12,0,c2.width-12,c2h)

  ctx.font = "bold 11px sans-serif"
  ctx.lineWidth = 1
  getLoHi()
  ctx.strokeStyle = "#000"
  ctx.fillStyle = "#000"
  if(avg){
   ctx.beginPath()
   m=c2.width-tm2x(itms[cnt-1][0])
   r=m+tm2x(itms[0][0])
   tt1=tween(+itms[cnt-1][1],+itms[0][1],m,r) // get y of midnight
   th1=tween(+itms[cnt-1][1]-itms[cnt-1][2],+itms[0][1]-itms[0][2],m,r) // thresh
   ctx.moveTo(12, t2y(tt1)) //1st point
   for(i=0;i<cnt;i++){
     x=tm2x(itms[i][0]) // time to x
     linePos(+itms[i][1])
   }
   i--;
   x=c2.width
   linePos(tt1) // temp at mid
   linePos(th1) // thresh tween
   for(;i>=0;i--){
    x=tm2x(itms[i][0])
    linePos(itms[i][1]-itms[i][2])
   }
   x=12
   linePos(th1) // thresh at midnight
   ctx.closePath()
   if(a.vo.value=='ON ')
     ctx.fillStyle = "rgba(130,130,130,0.3)" // thresholds (not active)
   else
     ctx.fillStyle = "rgba(80,80,80,0.5)" // thresholds
   ctx.fill()
   ctx.fillStyle = "#000" // temps
   for(i=0;i<cnt;i++){
    x=tm2x(itms[i][0]) // time to x
    ctx.fillText(itms[i][1],x,t2y(+itms[i][1])-4)
   }
  }else{
   for(i=0;i<cnt;i++)
   {
    x=tm2x(itms[i][0])
    y=t2y(itms[i][1]) // temp
    y2=t2y(itms[i][1]-itms[i][2])-y // thresh
    if(i==cnt-1) x2=c2.width-x
    else x2=tm2x(itms[i+1][0])-x
    if(a.vo.value=='ON ')
      ctx.fillStyle = "rgba(130,130,130,0.3)" // thresh
    else
      ctx.fillStyle = "rgba(80,80,80,0.5)" // thresh
    ctx.fillRect(x, y, x2, y2)
    ctx.fillStyle = "#000" // temp
    ctx.fillText(itms[i][1], x, y-2)
   }
   x0=tm2x(itms[0][0])
   ctx.fillStyle = "#666"
   ctx.fillRect(0, y, x0, y2) // rollover midnight
  }

  if(a.vo.value=='ON ')
  {
   ctx.fillStyle = "rgba(90,90,90,0.5)" // vacation thresh
   y=t2y(+a.vt.value)
   y2=t2y(+a.vt.value-1)
   ctx.fillRect(0, y, c2.width, y2)
  }

  ctx.fillStyle = "rgba(0,0,0,0.5)" // hrs
  step=c2.width/cnt
  y=c2h
  ctx.fillText(12,c2.width/2-6, y)//hrs
  ctx.fillText(6, c2.width/4-3, y)
  ctx.fillText(18,c2.width/2+c2.width/4-6, y)
  getLoHi()
  ctx.fillStyle = "#000"
  ctx.fillText(hi,0,8)
  ctx.fillText(lo,0,c2h+4)
  hl=hi-lo
  ctx.strokeStyle = "rgba(13,13,13,0.1)"
  step=c2h/hl
  ctx.beginPath()
  for(y=step,n=hi-1;y<c2h;y+= step){ // h-lines
    ctx.moveTo(14,y)
    ctx.lineTo(c2.width,y)
    ctx.fillText(n--,0,y+3)
  }
  ctx.stroke()
  step = (c2.width-12)/24
  ctx.beginPath()
  for(x=step;x<c2.width;x+=step){ // vertical lines
    ctx.moveTo(x,0)
    ctx.lineTo(x,c2h)
  }
  ctx.stroke()
  ctx.beginPath()
  ctx.strokeStyle="#0D0"
  for(i=0;i<tdata.length;i++) // rh
  {
   if(+tdata[i][0]==0){ctx.beginPath(); continue}
   x=tm2x(i*5)
   y=c2h-(tdata[i][3]/1000*c2h)
   ctx.lineTo(x,y)
   ctx.stroke()
   ctx.beginPath()
   if(tdata[i][1]!=8)
    ctx.moveTo(x,y)
  }

  ctx.beginPath()
  ctx.strokeStyle="#834"
  for(i=0;i<tdata.length;i++) // room temp
  {
   if(+tdata[i][0]==0){ctx.beginPath(); continue}
   x=tm2x(i*5)
   y=t2y(+tdata[i][2]/10)
   ctx.lineTo(x,y)
   ctx.stroke()
   ctx.beginPath()
   if(tdata[i][1]!=8)
     ctx.moveTo(x,y)
  }

  ctx.beginPath()
  for(i=0;i<tdata.length;i++) // WB temp
  {
   if(+tdata[i][0]==0){ctx.beginPath(); continue}
   x=tm2x(i*5)
   y=t2y(+tdata[i][0]/10)
   ctx.lineTo(x,y)
   ctx.stroke()
   ctx.beginPath()
   ctx.strokeStyle=(+tdata[i][1]&1)?'#F00':'#00F'
   if(tdata[i+1][0])
    ctx.moveTo(x,y)
  }

  for(i=0;i<tdata.length;i++) // presence
  {
   if(+tdata[i][1]&6==0) continue;
   x=tm2x(i*5)
   y=c2.height-9
   switch((+tdata[i][1]>>1)&3)
   {
      case 0: ctx.fillStyle="#000"; break
     case 1:
     case 2: ctx.fillStyle="#70F"; break
     case 3: ctx.fillStyle="#077"; break
   }
  ctx.fillRect(x,y, 3, 9);
  }

  dt=new Date()
  x=tm2x((dt.getHours()*60)+dt.getMinutes())
  y=t2y(waterTemp)
  ctx.beginPath()
  ctx.moveTo(x,y)
  ctx.lineTo(x-4,y+6)
  ctx.lineTo(x+4,y+6)
  ctx.closePath()
  ctx.fillStyle="#0f0" // arrow
  ctx.fill()
  ctx.stroke()
}catch(err){}
}
function linePos(t)
{
  ctx.lineTo(x,t2y(t))
}
function tm2x(t)
{
  return (t*(c2.width-12)/1440)+12
}
function t2y(t)
{
  return c2.height-10-((t-lo)/(hi-lo)*(c2.height-10))
}
function tween(t1, t2, m, r)
{
  t=(t2-t1)*(m*1000/r)/1000
  t+=t1
  return t.toFixed(1)
}
function getLoHi()
{
  lo=990
  hi=600
  for(i=0;i<cnt;i++){
    if(itms[i][1]*10>hi) hi=itms[i][1]*10
    if((itms[i][1]-itms[i][2])*10<lo) lo=(itms[i][1]-itms[i][2])*10
  }
  for(i=0;i<tdata.length;i++)
  {
    if(tdata[0] == 0) continue
    if(+tdata[i][0]>hi) hi=+tdata[i][0]
    if(+tdata[i][2]>hi) hi=+tdata[i][2]
    if(+tdata[i][0]>0&&+tdata[i][0]<lo) lo=+tdata[i][0]
    if(+tdata[i][2]>0&&+tdata[i][2]<lo) lo=+tdata[i][2]
  }
  lo=Math.floor(lo/10)
  hi=Math.ceil(hi/10)
}
function draw_bars(ar,pp)
{
  graph = $('#graph')
  var c=document.getElementById('graph')
  rect=c.getBoundingClientRect()
  canvasX=rect.x
  canvasY=rect.y

  tipCanvas=document.getElementById("tip")
  tipCtx=tipCanvas.getContext("2d")
  tipDiv=document.getElementById("popup")

  ctx=c.getContext("2d")
  ctx.fillStyle="#FFF"
  ctx.font="10px sans-serif"
  ctx.lineCap="round"

  dots=[]
  date=new Date()
  ht=c.height
  ctx.lineWidth=10
  draw_scale(ar,pp,c.width-4,ht-2,2,1,date.getMonth())

  // request mousemove events
  graph.mousemove(function(e){handleMouseMove(e);})

  // show tooltip when mouse hovers over dot
  function handleMouseMove(e){
    rect=c.getBoundingClientRect()
    mouseX=e.clientX-rect.x
    mouseY=e.clientY-rect.y
    var hit = false
    for(i=0;i<dots.length;i++){
      dot=dots[i]
      if(mouseX>=dot.x && mouseX<=dot.x2 && mouseY>=dot.y && mouseY<=dot.y2){
        tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
        tipCtx.fillStyle="#000000"
        tipCtx.strokeStyle='#333'
        tipCtx.font='italic 8pt sans-serif'
        tipCtx.textAlign="left"
        tipCtx.fillText(dot.tip, 4,15)
        tipCtx.fillText(dot.tip2,4,29)
        tipCtx.fillText(dot.tip3,4,44)
        popup=document.getElementById("popup")
        popup.style.top =(dot.y)+"px"
        x=dot.x-60
        if(x<10)x=10
        popup.style.left=x+"px"
        hit=true
      }
    }
    if(!hit){popup.style.left="-1000px"}
  }

  function getMousePos(cDom, mEv){
    rect = cDom.getBoundingClientRect();
    return{
     x: mEv.clientX-rect.left,
     y: mEv.clientY-rect.top
    }
  }
}

function draw_scale(ar,pp,w,h,o,p,ct)
{
  ctx.fillStyle="#336"
  ctx.fillRect(2,o,w,h-3)
  ctx.fillStyle="#FFF"
  max=0
  tot=0
  costTot=0
  for(i=0;i<ar.length;i++)
  {
    if(ar[i]>max) max=ar[i]
    tot+=ar[i]
  }
  ctx.textAlign="center"
  lw=ctx.lineWidth
  clr='#55F'
  mbh=0
  bh=0
  for(i=0;i<ar.length;i++)
  {
    x=i*(w/ar.length)+4+ctx.lineWidth
    ctx.strokeStyle='#55F'
    if(ar[i]){
        bh=ar[i]*(h-28)/max
        y=(o+h-20)-bh
      ctx.beginPath()
        ctx.moveTo(x,o+h-22)
        ctx.lineTo(x,y)
      ctx.stroke()
    }
    ctx.strokeStyle="#FFF"
    ctx.fillText(i+p,x,o+h-7)

    if(i==ct)
    {
      ctx.strokeStyle="#000"
      ctx.lineWidth=1
      ctx.beginPath()
        ctx.moveTo(x+lw+1,o+h-2)
        ctx.lineTo(x+lw+1,o+1)
      ctx.stroke()
      ctx.lineWidth=lw
    }
    bh=+bh.toFixed()+5
    x=+x.toFixed()
    cost=+(ppkw*ar[i]*(watts/3600000)).toFixed(2)
    costTot+=cost
    if(ar[i])
      dots.push({
      x: x-lw/2,
      y: (o+h-20)-bh,
      y2: (o+h),
      x2: x+lw/2,
      tip: t2hms(ar[i]),
      tip2: '$'+cost,
      tip3: '@ $'+ppkw
    })
  }
  ctx.fillText((tot*watts/3600000).toFixed(1)+' KWh',w/2,o+10)
  ctx.fillText('$'+costTot.toFixed(2),w/2,o+22)
}
function t2hms(t)
{
  s=t%60
  t=Math.floor(t/60)
  if(t==0) return s
  if(s<10) s='0'+s
  m=t%60
  t=Math.floor(t/60)
  if(t==0) return m+':'+s
  if(m<10) m='0'+m
  h=t%24
  t=Math.floor(t/24)
  if(t==0) return h+':'+m+':'+s
  return t+'d '+h+':'+m+':'+s
}

function draw_chart(){
try {
  var c=document.getElementById('chart')
  ctx=c.getContext("2d")
  while(va.length>65500) va.shift()
  ctx.fillStyle="#222"
  ctx.fillRect(0,0,c.width,c.height)
  ctx.fillStyle="#FFF"
  ctx.lineWidth=1
  ctx.font="bold 10px sans-serif"
  ctx.textAlign="right"
  h=c.height-60
  if(min2<0){
    base=h/2
    range=h/2
  }else{
    base=h
    range=h
  }
  bd=10
  base+=bd
  fVal=min2
  for(i=6;i>=0;i--)
  {
    y=(i/6*h)+bd
    ctx.strokeStyle="#FFF"
    ctx.fillText(fVal.toFixed(),20,y+3)
    fVal+=(max2-min2)/6
    ctx.strokeStyle="#555"
    ctx.beginPath()
    ctx.moveTo(21,y)
    ctx.lineTo(c.width,y)
    ctx.stroke()
  }

  ctx.strokeStyle = "#555"
  m=0
  ctx.font="10px sans-serif"
  ctx.fillText(zoom+':1',c.width-1,10)
  for(i=va.length-1-dispIdx,x=c.width-1;x>20&&i>=0;i-=zoom,x--)
  {
    if(x%100==21)
    {
      ctx.beginPath()
      ctx.moveTo(x,h+bd)
      ctx.lineTo(x,bd)
      ctx.stroke()
//      ctx.save()
//      ctx.translate(x+30, h+60)
//      ctx.rotate(0.9)
//      ctx.fillText(va[i][0].toLocaleTimeString(),0,0)
//      ctx.restore()
    }
  }

  ciel=max2
  for(line=1;line<9;line++)
  {
    start=0
    ctx.strokeStyle=colors[line-1]
    for(i=va.length-1-dispIdx,x=c.width-1;x>20&&i>=0;i-=zoom,x--)
    {
      if(typeof(va[i][line])!='undefined'){
        v0=va[i][line]
        y=base-(v0/ciel*range)
        if(!start){start=1;ctx.beginPath();ctx.moveTo(c.width-1,y)}
        else if(start)
        {
          if(zoom>1)
          {
            min2=max2=y
            for(j=i+1;j<i+zoom;j++)
            {
              y=base-(va[j][line]/ciel*range)
              if(y<min2) min2=y
              if(y>max2) max2=y
            }
            if(min2<y) ctx.lineTo(x,min2)
            if(max2>y) ctx.lineTo(x,max2)
          }
          ctx.lineTo(x,y)
        }
      }
      else if(start){ctx.stroke();start=0;}
    }
    if(start)ctx.stroke()
  }
}catch(err){}
}
</script>
<style type="text/css">
#wrapper {
  width: 100%;
  height: 100px;
  position: relative;
}
#wrapper2 {
  width: 100%;
  height: 360px;
  position: relative;
}
#graph {
  width: 100%;
  height: 100%;
  position: relative;
}
#chart{
  width: 100%;
  height: 100%;
  position: absolute;
  top: 0;
  left: 0;
}
#popup {
  position: absolute;
  top: -100px;
  left: -1000px;
  z-index: 10;
}
</style>
</head>
<body bgcolor="silver" onload="{
key=localStorage.getItem('key')
if(key!=null) document.getElementById('myKey').value=key
openSocket()
}">
<h3 align="center">WiFi Waterbed Heater</h3>
<table align=right>
<tr><td align=center id="time"></td><td align=center><div id="temp"></div> </td><td align=center><div id="tgt"></div></td></tr>
<tr><td align=center>Bedroom: </td><td align=center><div id="rt"></div></td><td align=center><div id="rh"></div> </td></tr>
<tr><td id="eta"></td><td> </td>
<td>Display:<input type="button" value="ON " id="OLED" onClick="{oled()}"></td></tr>
<tr><td colspan=2>Vacation <input id='vt' type=text size=2 value='-10'><input type='button' id='vo' onclick="{setVaca()}"> &nbsp &nbsp </td>
<td>Avg: <input type="button" value="OFF" id="AVG" onClick="{setavg()}"></td></tr>
<tr><td colspan=2></td>
<td>Eco: <input type="button" value="OFF" id="eco" onClick="{setEco()}"></td></tr>
<tr><td>Schedule <input id='inc' type='button' onclick="{setCnt(1)}">Count <input id='dec' type='button' onclick="{setCnt(-1)}"}></td>
<td> Temperature<br>Adjust</td>
<td><input type='button' value='Up' onclick="{setTemp(1)}"> <input type='button' value='All Up' onclick="{setAllTemp(1)}"><br><input type='button' value='Dn' onclick="{setTemp(-1)}"> <input type='button' value='All Dn' onclick="{setAllTemp(-1)}"></td></tr>
<tr>
<td align="left"></td><td>Time &nbsp;&nbsp;</td><td>Temp &nbsp;Thresh</td>
<tr><td id="s0"><input type="button" style="width:80px" value="Spring" onClick="{setSeason(0)}"></td><td colspan=2 id='r0' style="display:none"><input id=S0 type=text size=3> <input id=T0 type=text size=3> <input id=H0 type=text size=2></td></tr>
<tr><td id="s1"><input type="button" style="width:80px" value="Summer" onClick="{setSeason(1)}"></td><td colspan=2 id='r1' style="display:none"><input id=S1 type=text size=3> <input id=T1 type=text size=3> <input id=H1 type=text size=2></td></tr>
<tr><td id="s2"><input type="button" style="width:80px" value="Fall" onClick="{setSeason(2)}"></td><td colspan=2 id='r2' style="display:none"><input id=S2 type=text size=3> <input id=T2 type=text size=3> <input id=H2 type=text size=2></td></tr>
<tr><td id="s3"><input type="button" style="width:80px" value="Winter" onClick="{setSeason(3)}"></td><td colspan=2 id='r3' style="display:none"><input id=S3 type=text size=3> <input id=T3 type=text size=3> <input id=H3 type=text size=2></td></tr>
<tr><td></td><td colspan=2 id='r4' style="display:none"><input id=S4 type=text size=3> <input id=T4 type=text size=3> <input id=H4 type=text size=2></td></tr>
<tr><td></td><td colspan=2 id='r5' style="display:none"><input id=S5 type=text size=3> <input id=T5 type=text size=3> <input id=H5 type=text size=2></td></tr>
<tr><td></td><td colspan=2 id='r6' style="display:none"><input id=S6 type=text size=3> <input id=T6 type=text size=3> <input id=H6 type=text size=2></td></tr>
<tr><td></td><td colspan=2 id='r7' style="display:none"><input id=S7 type=text size=3> <input id=T7 type=text size=3> <input id=H7 type=text size=2></td></tr>
<tr><td colspan=3><canvas id="canva" width="300" height="150" style="float:center" onclick="draw() /"></td></tr>
<tr><td colspan=3>
<div id="wrapper">
<canvas id="graph" width="300" height="100" />
<div id="popup"><canvas id="tip" width="70" height="48" /></div>
</div>
</td></tr>
<tr><td colspan=2 align="left">PPKWH $<input id='K' type=text size=2 value='0.1457' onchange="{setPPK()}">
 </td><td> <input id="myKey" name="key" type=text size=40 placeholder="password" style="width: 100px" onChange="{localStorage.setItem('key', key = document.all.myKey.value)}"></td></tr>
<tr>
<tr>
<td colspan=3>
<div id="dropContainer">
<table style="font-size:small" id="fileList">
<p align="left" id="free">Internal: 0K Free</p>
</table>
</div>
</td>
</tr>
<tr>
<td colspan=3>
<div id="dropContainerSD">
<table style="font-size:small" id="fileListSD">
<p align="left" id="freeSD">SDCard: 0K Free</p>
</table>
</div>
</td>
</tr>
</table>
</body>
<script type="text/javascript">
dropContainer.ondragover = dropContainer.ondragenter = function(ev){ev.preventDefault()}
dropContainer.ondragend = dropContainer.ondraleave = function(ev){ev.preventDefault()}

dropContainer.ondrop = function(evt) {
  data = evt.dataTransfer.files[0]
  formData = new FormData()
  formData.append("entry", data)
  fetch('/upload_internal', {method: 'POST', body: formData})
  evt.preventDefault()
  it=new Array()
  it[0]=data.name
  it[1]=data.size
  AddFile(it)
}
dropContainerSD.ondragover = dropContainerSD.ondragenter = function(ev){ev.preventDefault()}
dropContainerSD.ondragend = dropContainerSD.ondraleave = function(ev){ev.preventDefault()}

dropContainerSD.ondrop = function(evt) {
  data = evt.dataTransfer.files[0]
  formData = new FormData()
  formData.append("entry", data)
  fetch('/upload_sd', {method: 'POST', body: formData})
  evt.preventDefault()
  it=new Array()
  it[0]=data.name
  it[1]=data.size
  AddFileSD(it)
}
</script>
</html>
)rawliteral";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};

const uint8_t delbtn_png[] PROGMEM = {
  0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
  0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x18,0x08,0x06,0x00,0x00,0x00,0xE0,0x77,0x3D,
  0xF8,0x00,0x00,0x00,0x06,0x62,0x4B,0x47,0x44,0x00,0xFF,0x00,0xFF,0x00,0xFF,0xA0,
  0xBD,0xA7,0x93,0x00,0x00,0x00,0x09,0x70,0x48,0x59,0x73,0x00,0x01,0x03,0x85,0x00,
  0x01,0x03,0x85,0x01,0xA5,0x54,0x3C,0xFE,0x00,0x00,0x00,0x07,0x74,0x49,0x4D,0x45,
  0x07,0xE2,0x09,0x13,0x03,0x36,0x36,0x9C,0x04,0x30,0x9C,0x00,0x00,0x04,0x5D,0x49,
  0x44,0x41,0x54,0x48,0xC7,0x9D,0x96,0x4D,0x4C,0x54,0x57,0x14,0xC7,0x7F,0xEF,0xCD,
  0x7B,0x0C,0xC3,0x0C,0x71,0xD0,0x37,0xDF,0x32,0xA3,0x4C,0x5D,0x68,0x6C,0xA2,0xD1,
  0x06,0xB5,0xB1,0x8D,0xA6,0x60,0x02,0x89,0x4D,0x93,0xC6,0x90,0x28,0x4D,0xD7,0xB8,
  0xE9,0xC7,0xCA,0xBA,0x60,0x55,0x91,0x85,0x13,0x36,0xED,0x9A,0xF0,0x91,0xD8,0xC4,
  0xC4,0x48,0xA2,0x01,0x8C,0x60,0xDB,0x54,0xDB,0xA4,0x24,0x5D,0x98,0x71,0x81,0xDA,
  0x60,0x90,0x01,0xC1,0xAF,0x16,0x67,0x78,0x33,0x6F,0xDE,0xE9,0xE2,0x31,0x0C,0x03,
  0x94,0x5A,0x4F,0xF2,0x36,0xE7,0xBE,0x73,0xCE,0xFF,0xDE,0xF3,0xBF,0xFF,0x73,0x15,
  0x36,0xB7,0x08,0xF0,0x09,0x70,0xC2,0xAD,0x28,0x7B,0x0D,0x45,0x4D,0x00,0x2C,0x88,
  0x3D,0x65,0x8A,0xDC,0x03,0x46,0x80,0xAB,0x40,0x86,0xFF,0x69,0x31,0xA0,0x2F,0xE9,
  0xD2,0xA4,0xD3,0xE7,0x97,0xC9,0x40,0x54,0x24,0xD6,0x20,0x12,0xDB,0xB9,0xFC,0x35,
  0xC8,0x64,0x20,0x2A,0x9D,0x3E,0xBF,0x24,0x5D,0x9A,0x00,0x7D,0xCB,0x31,0xEB,0x4C,
  0xD9,0xC0,0xD7,0xEE,0x53,0xD4,0xDE,0xA1,0x3A,0x43,0x3D,0xE6,0xAE,0x01,0xB1,0x37,
  0x87,0xA2,0xA8,0x8C,0x9B,0x59,0x4E,0xBE,0x58,0xB0,0x17,0xC5,0xFE,0x1C,0xE8,0xDF,
  0xAC,0xC0,0x77,0xBB,0x35,0xBD,0x23,0x6D,0x44,0x78,0x1B,0xDB,0xB3,0x90,0xE1,0xBE,
  0x55,0xF8,0x1E,0x38,0x5B,0xF2,0xA9,0xAB,0x91,0xEF,0xD6,0xF4,0x8E,0x74,0x20,0xBA,
  0x5C,0x5A,0x61,0xFF,0xDC,0x34,0xE3,0x79,0xF3,0x5F,0x13,0x8E,0xE7,0x4D,0xF6,0xCF,
  0x4D,0x83,0xE2,0xE0,0x4C,0x07,0xA2,0xEC,0xD6,0xF4,0x0E,0xA0,0x7D,0x6D,0x81,0x98,
  0x4F,0x51,0x7B,0xD3,0x46,0x04,0x44,0x40,0x51,0x38,0x94,0x99,0xE2,0xD4,0xC5,0x2E,
  0x4E,0xD7,0xBA,0x19,0x31,0x73,0xEB,0x92,0x8F,0x9A,0x39,0x4E,0xD7,0xBA,0x39,0x75,
  0xB1,0x8B,0x43,0x99,0x29,0xA7,0x88,0x08,0x69,0x23,0x82,0x4F,0x51,0x7B,0xD7,0xF6,
  0xA4,0x6F,0x6C,0x6B,0x50,0x24,0x1C,0x17,0x89,0x24,0xA4,0x11,0xA4,0x3B,0x95,0x12,
  0x11,0x11,0xD3,0x34,0x25,0x94,0x4C,0xCA,0xB0,0xDF,0x10,0x09,0x27,0x44,0xC2,0x09,
  0x19,0xF6,0x1B,0x12,0x4A,0x26,0xC5,0x34,0x4D,0x11,0x11,0xE9,0x4E,0xA5,0xA4,0x11,
  0x44,0x22,0x09,0x91,0x70,0x5C,0xC6,0xB6,0x06,0x4B,0x8D,0x77,0xA8,0x98,0x74,0x69,
  0x22,0x91,0x1D,0x22,0xE1,0xB8,0xEC,0x53,0x54,0xB9,0xD0,0xDD,0x2D,0xAB,0xAD,0x68,
  0x59,0xB2,0x25,0x5E,0x2F,0xB7,0xEB,0x82,0x72,0xBB,0x2E,0x28,0x5B,0xE2,0xF5,0x52,
  0xB4,0xAC,0x8A,0x7F,0x2E,0x74,0x77,0xCB,0x3E,0x45,0x5D,0x06,0xB9,0xA3,0xC4,0xAE,
  0x08,0x40,0x47,0xA7,0xCF,0xEF,0x2C,0x84,0xE3,0x32,0xB6,0x35,0x24,0x91,0x44,0x7C,
  0x05,0xDD,0x6A,0x33,0x42,0x21,0x31,0x42,0xA1,0x75,0x7E,0xD3,0x34,0x25,0x92,0x70,
  0x62,0x4B,0x79,0x3A,0x7D,0x7E,0x01,0x3A,0x00,0xAE,0x4D,0x06,0xA2,0x2B,0x0B,0x12,
  0xAA,0x97,0x61,0xBF,0x21,0xD1,0x5D,0xBB,0xC4,0x2E,0x16,0x2B,0x12,0x65,0xB3,0x59,
  0xC9,0x66,0xB3,0xEB,0x76,0x17,0x4A,0x26,0x65,0xB8,0x2E,0x50,0xCE,0x11,0x8E,0xCB,
  0x64,0x20,0x2A,0xC0,0x35,0xC5,0xAD,0x28,0x0F,0x97,0x22,0x89,0x06,0xC4,0x86,0x60,
  0x08,0x7C,0x5E,0xA8,0xAE,0xE6,0x8E,0x99,0xE3,0x4B,0x8F,0xCE,0x6F,0xA3,0x37,0x37,
  0xA5,0xA6,0x3F,0x11,0xE7,0x87,0xBF,0x72,0x9C,0xA8,0xAE,0xA9,0x5C,0x70,0x69,0x54,
  0xCF,0xFC,0xF9,0x48,0x89,0xB9,0x34,0x6B,0xFA,0xBD,0x23,0x2E,0x7C,0xDE,0x15,0x26,
  0x94,0xEC,0xF7,0x82,0xC9,0xD7,0x2E,0x18,0xBD,0x71,0x03,0xB7,0xDB,0x5D,0x11,0x9F,
  0xCB,0xE5,0x88,0xEF,0xDC,0xC9,0x95,0xBC,0xF0,0xA1,0xDB,0x03,0x48,0x65,0x01,0xD5,
  0xC5,0xF6,0xD9,0xC7,0x45,0x15,0x4D,0x03,0x63,0x9B,0xE3,0x14,0x79,0x8B,0xEB,0xB5,
  0x26,0x66,0x99,0xE6,0x04,0x83,0xA0,0xB9,0x50,0x17,0x0A,0xF9,0xA9,0x8D,0x14,0xE3,
  0xAE,0xB9,0xC4,0xD9,0x2A,0x95,0x1F,0x6F,0xDD,0x5A,0x87,0x1E,0xC0,0xE3,0xF1,0x30,
  0x3F,0x3B,0xCB,0xC7,0xB5,0x1E,0x46,0x97,0xB2,0xA0,0xAA,0xE0,0xF5,0x42,0x34,0x0A,
  0xEF,0xEE,0x85,0xFA,0x18,0x0B,0x96,0x35,0xA5,0x99,0xB6,0x7D,0x6F,0xF2,0xC5,0xB3,
  0x86,0x5D,0x9E,0xF2,0x19,0xDE,0x29,0x98,0x9C,0xF7,0xBA,0xF9,0xF5,0xFA,0xF5,0x8A,
  0xA4,0xA6,0xE9,0xDC,0xEA,0xD5,0x05,0x9F,0x3F,0x7A,0xC4,0xD1,0xA6,0x8F,0xA8,0x15,
  0x8D,0xC3,0x7A,0x95,0xB3,0x03,0xDB,0xE6,0xC1,0xF3,0x67,0x98,0xB6,0x7D,0x4F,0x05,
  0x46,0x06,0xE7,0xCA,0x6A,0x3B,0x61,0xE5,0x39,0x5F,0xAD,0x33,0x3A,0x34,0x84,0xA2,
  0xAA,0x15,0x05,0x9A,0x5B,0x5A,0x68,0x6E,0x69,0x59,0x73,0xD4,0x2E,0x6E,0x8F,0x8C,
  0xF2,0x8D,0xDB,0xC5,0x44,0xA1,0x2C,0x2B,0x03,0x4E,0xCE,0x11,0x15,0xB8,0x3A,0x30,
  0x33,0x0D,0x9A,0x06,0x40,0xD7,0xAB,0x97,0xB4,0xB7,0xB5,0xA1,0xEB,0x7A,0xF9,0x58,
  0x6D,0x9B,0xC6,0xE6,0x26,0x2E,0x15,0xE1,0x52,0x11,0x1A,0x9B,0x9B,0x10,0xBB,0xAC,
  0xB2,0xBA,0xAE,0xD3,0xDE,0xD6,0x46,0xD7,0xAB,0x97,0x8E,0x43,0xD3,0x18,0x98,0x99,
  0x06,0xB8,0xAA,0x02,0x99,0x87,0xD9,0x6C,0xFF,0xF8,0xFC,0x53,0x00,0xAE,0x18,0x21,
  0xC6,0x52,0x3D,0xF4,0x0E,0x0E,0x02,0x50,0x28,0x14,0x38,0xDE,0xDA,0x4A,0xCF,0x52,
  0x91,0x83,0xBA,0x9B,0x83,0xBA,0x9B,0x9E,0xA5,0x22,0xC7,0x5B,0x5B,0x29,0x14,0x0A,
  0x00,0xF4,0x0E,0x0E,0x32,0x96,0xEA,0xE1,0x8A,0x11,0x72,0x44,0x70,0xFE,0x29,0x0F,
  0xB3,0xD9,0x7E,0x20,0xA3,0xAC,0x88,0x9D,0x4B,0x7B,0xFC,0xF7,0xB1,0x26,0xB5,0xA4,
  0xA4,0x67,0x66,0x9F,0x70,0xFC,0xAB,0x2F,0xE8,0xBF,0x7C,0x99,0x6F,0x97,0x0A,0x1C,
  0xD1,0x2B,0x1B,0x7D,0xA7,0x60,0x72,0xBE,0xDA,0x41,0x3E,0x96,0xEA,0x61,0x20,0x1C,
  0x5B,0x61,0x61,0xED,0xF8,0x4D,0x7B,0xB1,0x68,0xC5,0x81,0x27,0x4A,0x85,0x5C,0x7B,
  0x7D,0x7D,0xE9,0xF7,0x3F,0x00,0xDB,0xA1,0xDA,0xA7,0x0B,0x73,0x9C,0xDB,0xE2,0xE7,
  0x80,0x56,0xB5,0x21,0x41,0x27,0xAC,0x3C,0x5D,0xAF,0x5E,0x3A,0xC8,0x45,0x40,0x55,
  0xD8,0xF3,0xCB,0x4F,0xDC,0x7F,0xBD,0xF8,0x59,0x69,0xF0,0xAC,0x1F,0x38,0x5E,0x5F,
  0x47,0xFA,0xF0,0xD1,0xB7,0x1B,0x38,0x77,0x7F,0xE6,0xFE,0xEB,0xC5,0x8A,0x81,0xB3,
  0xF1,0xC8,0x74,0x69,0xBD,0x43,0xFB,0x0E,0xA8,0xC7,0x02,0x41,0xB0,0xAC,0xCD,0xB3,
  0x6A,0x1A,0xE3,0xF3,0x4F,0x39,0xF9,0xC7,0x84,0xBD,0x58,0xB4,0xFE,0x73,0x64,0xAE,
  0x1E,0xFA,0x5D,0xC9,0x9A,0x9A,0xF6,0x33,0xD1,0xED,0x9C,0x09,0x45,0x78,0xA7,0x6E,
  0x5B,0xF9,0xA6,0x2B,0x0A,0x0F,0x5E,0x3C,0x63,0x60,0x2E,0xC3,0xC0,0xCC,0x74,0xA9,
  0xA1,0xE7,0x80,0x27,0x6F,0x32,0xF4,0x37,0x7E,0xB6,0xA8,0xEA,0x5E,0x43,0xAF,0x72,
  0x9E,0x2D,0x85,0xFC,0x94,0x69,0xDB,0x6F,0xF4,0x6C,0xF9,0x07,0x37,0xB8,0x4A,0x73,
  0x18,0x72,0xA3,0x13,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82,
};
