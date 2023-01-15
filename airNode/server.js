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
let i = 1;
function update() {
    request.get({url:'http://192.168.2.97/'}, function(err,httpResponse,body){
	if(err){
		return;
	}else {

        data = JSON.parse(body);
        if(data['pm2.5']>400){
            return;
        }
        data.time = new Date().toString();
        if(data["IP"]){
            delete data["IP"];
        }
        if(data["SSID"]) {
            delete data["SSID"];
        }
        data = JSON.stringify(data).replace(/\r/g,"");
        if(io.engine.clientsCount>0) {
            io.emit('data', data);
        }
	if(i%2==0) {
		console.log(data);
		i = 1;
	}
	else {
		i = i + 1;
	}

	}
    });
}

setInterval(update,5000);
update();

server.listen(3333);
