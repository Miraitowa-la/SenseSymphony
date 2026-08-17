const assert = require('assert')
const source = require('fs').readFileSync('pages/scores/scores.vue', 'utf8')
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1].replace(/^\s*import .*$/gm, '').replace('export default', 'return')
const component = new Function(script)()

assert.strictEqual(component.methods.songName.call({ songs: [{ id: 1, name: 'ODE_TO_JOY' }] }, 1), 'ODE_TO_JOY')
console.log('mode2 song-name self-check passed')
