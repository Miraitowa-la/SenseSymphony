const assert = require('assert')
const source = require('fs').readFileSync('pages/record/record.vue', 'utf8')

assert(source.includes('width="650" height="270"'), 'canvas buffer matches draw coordinates')
console.log('trace canvas self-check passed')
