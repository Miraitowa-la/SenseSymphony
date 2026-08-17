const assert = require('assert')
const source = require('fs').readFileSync('pages/songs/songs.vue', 'utf8')
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1].replace(/^\s*import .*$/gm, '').replace('export default', 'return')
const component = new Function(script)()

assert.strictEqual(component.methods.songKind(0), '内置曲目')
assert.strictEqual(component.methods.songKind(16), '我的曲目')
console.log('song kind self-check passed')
