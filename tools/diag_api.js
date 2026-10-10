const https = require('https');
const t0 = Date.now();
const req = https.get('https://api.deepseek.com/v1/models', {
  headers: { 'Authorization': 'Bearer sk-diag-invalid', 'User-Agent': 'dsh-diag' },
  timeout: 20000
}, (res) => {
  let body = '';
  res.on('data', c => body += c);
  res.on('end', () => {
    console.log('STATUS', res.statusCode, 'in', Date.now() - t0, 'ms');
    console.log('BODY', body.slice(0, 300));
    process.exit(0);
  });
});
req.on('timeout', () => { console.log('TIMEOUT after', Date.now() - t0, 'ms'); process.exit(2); });
req.on('error', (e) => { console.log('ERROR', e.code || e.name, e.message); process.exit(1); });
