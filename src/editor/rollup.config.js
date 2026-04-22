import resolve from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import postcss from 'rollup-plugin-postcss';
import copy from 'rollup-plugin-copy';

export default {
  input: 'index.js',
  output: {
    file: 'dist/cyth.js',
    format: 'esm',
  },
  plugins: [
    {
      name: 'disable-node-builtins',
      resolveId(id) {
        if (['fs', 'path', 'crypto', 'os', 'stream', 'util', 'buffer'].includes(id)) return '\0' + id;
      },
      load(id) {
        if (id.startsWith('\0')) return 'export default {}';
      },
    },
    resolve({ browser: true, }),
    postcss({ inject: true, minimize: true }),
    terser({
      mangle: {
        reserved: ['Module'],
      },
    }),
    copy({
      targets: [
        { src: 'index.html', dest: 'dist' },
        { src: 'cyth.wasm', dest: 'dist' },
        { src: 'worker.js', dest: 'dist' },
        { src: 'codicon.ttf', dest: 'dist' },
      ],
    }),
  ],
};