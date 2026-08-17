const assert = require('assert')
const source = require('fs').readFileSync('services/spp.js', 'utf8')

assert(source.includes('连接失败：'), 'show one-line connection diagnostic')
assert(source.includes('slice(0, 90)'), 'bound diagnostic length')
console.log('SPP diagnostic self-check passed')
