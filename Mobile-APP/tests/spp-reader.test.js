const assert = require('assert')
const source = require('fs').readFileSync('services/spp.js', 'utf8')

assert(source.includes('input.available()'), 'poll only bytes already available')
assert(!source.includes('while ((byte = input.read()) >= 0)'), 'do not block the JS runtime in a reader loop')
console.log('SPP reader self-check passed')
