const assert = require('assert')
const source = require('fs').readFileSync('services/spp.js', 'utf8')

assert(source.includes('createRfcommSocketToServiceRecord'), 'try secure SPP first')
assert(source.includes('createInsecureRfcommSocketToServiceRecord'), 'fall back to compatible SPP')
assert((source.match(/plus\.android\.importClass\(socket\)/g) || []).length >= 2, 'import each native socket before use')
console.log('SPP connection fallback self-check passed')
