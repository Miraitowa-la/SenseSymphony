const assert = require('assert')
const fs = require('fs')

import('data:text/javascript,' + encodeURIComponent(fs.readFileSync('utils/protocol.js', 'utf8'))).then(({ FrameBuffer }) => {
	const frames = new FrameBuffer()
	assert.deepStrictEqual(frames.push('M1,LIST,2\nM1,ITEM,0,7'), [{ type: 'list', count: 2 }])
	assert.deepStrictEqual(frames.push(',3\n\nM1,END\nNOPE,x\n'), [{ type: 'item', slot: 0, sequence: 7, eventCount: 3 }, { type: 'end' }])
	assert.deepStrictEqual(frames.push('M1,OK,DELETE,0\nM1,ERR,NOT_FOUND\n'), [{ type: 'delete_ok', slot: 0 }, { type: 'error', reason: 'NOT_FOUND' }])
	assert.deepStrictEqual(frames.push('M2,ITEM,3,9,0,9850,42,97,30,8,3,1\nM2,OK,DELETE,3\nM2,ERR,NOT_FOUND\n'), [
		{ type: 'mode2_item', slot: 3, sequence: 9, songId: 0, score: 9850, maxCombo: 42, accuracy: 97, perfect: 30, great: 8, good: 3, miss: 1 },
		{ type: 'mode2_delete_ok', slot: 3 },
		{ type: 'mode2_error', reason: 'NOT_FOUND' }
	])
	assert.deepStrictEqual(frames.push('M2,SONGS,4,3\nM2,SONG,0,TWINKLE\nM2,SONG,1,ODE_TO_JOY\nM2,SONG,2,MARY_HAD_A_LITTLE_LAMB\nM2,SONG,3,JINGLE_BELLS\nM2,OK,SELECT,1\nM2,ERR,BUSY\nM2,ERR,NOT_FOUND\n'), [
		{ type: 'mode2_songs', count: 4, selectedId: 3 },
		{ type: 'mode2_song', id: 0, name: 'TWINKLE' },
		{ type: 'mode2_song', id: 1, name: 'ODE_TO_JOY' },
		{ type: 'mode2_song', id: 2, name: 'MARY_HAD_A_LITTLE_LAMB' },
		{ type: 'mode2_song', id: 3, name: 'JINGLE_BELLS' },
		{ type: 'mode2_select_ok', id: 1 },
		{ type: 'mode2_error', reason: 'BUSY' },
		{ type: 'mode2_error', reason: 'NOT_FOUND' }
	])
	assert.deepStrictEqual(frames.push('M2,OK,UPLOAD_BEGIN\nM2,OK,NOTE,0\nM2,OK,UPLOAD,16\nM2,OK,SONG_DELETE,16\nM2,OK,CANCEL\nM2,ERR,FULL\nM2,ERR,INVALID\nM2,ERR,INCOMPLETE\nM2,ERR,READONLY\n'), [
		{ type: 'mode2_upload_begin_ok' }, { type: 'mode2_upload_note_ok', index: 0 }, { type: 'mode2_upload_ok', songId: 16 }, { type: 'mode2_song_delete_ok', songId: 16 }, { type: 'mode2_upload_cancel_ok' },
		{ type: 'mode2_error', reason: 'FULL' }, { type: 'mode2_error', reason: 'INVALID' }, { type: 'mode2_error', reason: 'INCOMPLETE' }, { type: 'mode2_error', reason: 'READONLY' }
	])
	assert.deepStrictEqual(frames.push('M2,OK,UPLOAD_BEGIN\nM2,OK,NOTE,0\nM2,OK,UPLOAD,16\nM2,OK,SONG_DELETE,16\nM2,OK,CANCEL\nM2,ERR,FULL\nM2,ERR,INVALID\nM2,ERR,INCOMPLETE\nM2,ERR,READONLY\n'), [
		{ type: 'mode2_upload_begin_ok' }, { type: 'mode2_upload_note_ok', index: 0 }, { type: 'mode2_upload_ok', songId: 16 }, { type: 'mode2_song_delete_ok', songId: 16 }, { type: 'mode2_upload_cancel_ok' },
		{ type: 'mode2_error', reason: 'FULL' }, { type: 'mode2_error', reason: 'INVALID' }, { type: 'mode2_error', reason: 'INCOMPLETE' }, { type: 'mode2_error', reason: 'READONLY' }
	])
	console.log('protocol self-check passed')
})
