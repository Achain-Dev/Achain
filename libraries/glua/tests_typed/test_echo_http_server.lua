let http = require 'http'
let net = require 'net'

let res = http.request('GET', "http://www.gov.cn/", '', {
	Accept="text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8",
	["User-Agent"]="Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.103 Safari/537.36"
})
pprint(http.get_res_header(res, "Content-Length"))
pprint(http.get_res_body(res))
http.close(res)

let server = http.listen("0.0.0.0", 3000)

pprint("listening on 0.0.0.0:3000\n")

-- async api

let function handler(ctx)
	let net = require 'net'
	pprint("got new connection", ctx)
	-- pprint('get req body', http.get_req_body(ctx), '\n')
	net.write(ctx, "HTTP/1.1 200 OK\r\nContent-Type:text/html; utf-8\r\nContent-Length:5\r\n\r\nhello")
	net.close_socket(ctx)
end

http.accept_async(server, handler)
http.start_io_loop(server)

pprint("starting sync http server")

while true do
    let ctx = http.accept(server)
    pprint('get req body', http.get_req_body(ctx), '\n')
    http.write_res_body(ctx, "hello world")
    http.set_status(ctx, 200, 'OK')
    http.set_res_header(ctx, "Content-Type", "text/html; utf-8")
    http.finish_res(ctx)
	http.close(ctx)
end
