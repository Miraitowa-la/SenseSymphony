const assert = require('assert')
const source = require('fs').readFileSync('pages/song-editor/song-editor.vue', 'utf8')

assert(/f\.index!==this\.uploadIndex\s*\+\s*1/.test(source), 'device NOTE acknowledgement is one-based')
console.log('upload acknowledgement index self-check passed')
