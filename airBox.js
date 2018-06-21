const Koa = require('koa');
const app = new Koa();
const request = require('request');
var data;

// response
app.use(ctx => {
    ctx.body = data;
});

function update() {
    request.get({url:'http://192.168.10.96/'}, function(err,httpResponse,body){

        data = JSON.parse(body);
        data.time = new Date().toString();
    })
}

setInterval(update,10000);
update();

app.listen(3333);
