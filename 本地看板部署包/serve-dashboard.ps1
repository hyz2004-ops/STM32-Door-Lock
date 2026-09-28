# 智能门锁看板 HTTP 服务器
# 作用: 让手机能打开看板页面(手机连电脑热点后访问 http://192.168.137.1:8000)
# 只服务本目录的 index.html / mqtt.min.js, 监听热点网卡 IP, 后台常驻
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$port = 8000

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://+:${port}/")   # '+' 需一次性 urlacl 授权(已配置), 热点开不开都能用
try { $listener.Start() } catch { exit 1 }

$mimes = @{ '.html' = 'text/html; charset=utf-8'; '.js' = 'application/javascript; charset=utf-8' }
try {
    'started ' + (Get-Date -Format 'HH:mm:ss') | Out-File (Join-Path $root 'serve.log') -Encoding utf8
    while ($listener.IsListening) {
    try {
        $ctx = $listener.GetContext()
        $path = $ctx.Request.Url.LocalPath.TrimStart('/')
        if ($path -eq '') { $path = 'index.html' }
        # 防目录穿越: 只允许白名单文件
        if ($path -notin @('index.html', 'mqtt.min.js')) {
            $ctx.Response.StatusCode = 404; $ctx.Response.Close(); continue
        }
        $file = Join-Path $root $path
        if (-not (Test-Path $file)) {
            $ctx.Response.StatusCode = 404; $ctx.Response.Close(); continue
        }
        $buf = [System.IO.File]::ReadAllBytes($file)
        $ctx.Response.ContentType = $mimes[[System.IO.Path]::GetExtension($path)]
        $ctx.Response.ContentLength64 = $buf.Length
        $ctx.Response.OutputStream.Write($buf, 0, $buf.Length)
        $ctx.Response.OutputStream.Close()
    } catch { }
}
