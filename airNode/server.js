const Koa = require('koa');
const http = require('http');
const app = new Koa();
const server = require('http').createServer(app.callback())
const io = require('socket.io')(server)
const request = require('request');
const fs = require('fs');
let data;


io.on('connection', function(socket){
  //console.log('a user connected');
  socket.emit('data',data);
});

let file = fs.readFileSync('public/monitor.html',"utf-8");
// response
app.use(ctx => {
	if(ctx.path=="/monitor"){    
		ctx.type = "html";
		ctx.body = file;
	}else {
		ctx.body = data;
	}
});

function update() {
    request.get({url:'http://192.168.10.213/'}, function(err,httpResponse,body){
	if(err){
		return;
	}else {
    
        data = JSON.parse(body);
	if(data['pm2.5']>500){
		return;
	}
	data.time = new Date().toString();
	data = JSON.stringify(data).replace(/\r/g,"");	
	if(io.engine.clientsCount>0) {
		io.emit('data', data);
	}
	console.log(data);
	}
    })
}

setInterval(update,10000);
update();

server.listen(3333);
