// Waterbed stream listener using PngMagic (http://www.curioustech.net/pngmagic.html)
wbIP = '192.168.31.8'
Url = 'ws://' + wbIP + '/ws'
var last
password = 'password'


heartbeat = (new Date()).valueOf() / 1000
updater = (new Date()).valueOf() / 1000

if(!Http.Connected)
{
	Http.Connect( 'event', Url )  // Start the event stream
}

Pm.SetTimer(1000)

// Handle published events
function OnCall(msg, event, data)
{
	switch(msg)
	{
		case 'PPKWH':
			Http.Send( '{key:' + password + ',ppkwh:' + event + '}'  )
			break
		case 'NOTIFY':
			Http.Send( '{key:' + password + ',notif:"' + event + '"}'  )
			break
		case 'NOTIFYCAN':
			Http.Send( '{key:' + password + ',notifCancel:"' + event + '"}'  )
			break
		case 'TEMP':
			t = +event
			rh = +data
			Http.Send( '{key:' + password + ',outtemp:' + t + ',outrh:' + rh+ '}'  )
			break
		case 'HTTPCONNECTED':
			Pm.Echo('WB3 connected')
			sendStuff()
			break
		case 'HTTPDATA':
			heartbeat = new Date()
//Pm.Echo('WB2 ' + data)
			procLine(data)
			break
		case 'HTTPCLOSE':
			Pm.Echo('WB3 disconnected')
			heartbeat = 0
//			Http.Connect( 'event', Url )  // Start the event stream
			break
		case 'HTTPSTATUS':
			Pm.Echo('WB3 HTTP status ' + data)
			break
	}
}

function procLine(data)
{
Pm.Echo(data)
	if(data.length < 2) return
	data = data.replace(/\n|\r/g, "")
	js = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		data.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + data + ')')

	switch(js.cmd)
	{
		case 'state':
			LogWB(data)
//			Pm.Server( 'WB3', data)
			break
		case 'alert':
			Pm.Echo('WB3 Alert: ' + data)
			Pm.Beep(0)
			break
		case 'print':
			Pm.Echo('WB3 ' + data)
			break
		case 'STAT':
			switch(js.value)
			{
				case 1:
					Pm.Echo('WB3 Fan toggle')
					break
				case 2:
					Pm.Echo('WB3 Temp down')
					break
				case 3:
					Pm.Echo('WB3 Fan toggle')
					break
			}
			break;
		case 'hack':
			Pm.Echo('WB3 Hack: ' + js.ip + ' ' + js.pass)
			break
	}
}

function sendStuff()
{
	s = '{'
	s +='"key":' + password + ','
	s +='"STATTEMP":' + Reg.statTemp + ','
	s +='"STATSETTEMP":' + Reg.statSetTemp + ','
	s +='"STATFAN":' + Reg.statFan + ','
	s +='"outtemp":' + Reg.outTemp*10 + ','
	s += '}'
	Http.Send(s)
}

function OnTimer()
{
	time = (new Date()).valueOf() / 1000
	if(time - heartbeat > 120)
	{
		if(!Http.Connected)
			Http.Connect( 'event', Url )  // Start the event stream
		heartbeat = time
	}

	if(time - updater > 10)
	{
		sendStuff()
		updater = time
	}
}

function 	LogWB(str)
{
	wbJson = !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
		str.replace(/"(\\.|[^"\\])*"/g, ''))) && eval('(' + str + ')')

	line = wbJson.waterTemp + ',' + wbJson.hiTemp + ',' + wbJson.loTemp + ',' + wbJson.on+ ',' + wbJson.temp+','+wbJson.rh

//	if(+wbJson.temp == 6553.5 || +wbJson.temp == 0)
//		return

	if(line == last)
		return
	last = line
//Pm.Echo('WB3 ' + (new Date())  + ' ' + str)
	Pm.Log( 'Waterbed3.log', wbJson.t + ',' + line + ',' + wbJson.tc)
}
