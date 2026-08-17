const assert = require('assert')
const source = require('fs').readFileSync('services/spp.js', 'utf8')

assert(source.includes("const devices = adapter.getBondedDevices()"), 'read bonded set before iterating')
assert(source.includes('plus.android.importClass(devices)'), 'import Java Set before calling iterator')
console.log('SPP bridge self-check passed')
