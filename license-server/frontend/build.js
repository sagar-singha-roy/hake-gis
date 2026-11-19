const esbuild = require('esbuild');
const fs = require('fs');
const path = require('path');

async function build() {
  console.log('Building React Admin Frontend...');
  const result = await esbuild.build({
    entryPoints: [path.join(__dirname, 'src/main.jsx')],
    bundle: true,
    minify: true,
    loader: { '.js': 'jsx', '.jsx': 'jsx' },
    define: { 'process.env.NODE_ENV': '"production"' },
    write: false,
    outdir: 'out'
  });

  let jsContent = '';
  let cssContent = '';

  for (const out of result.outputFiles) {
    if (out.path.endsWith('.js')) {
      jsContent = out.text;
    } else if (out.path.endsWith('.css')) {
      cssContent = out.text;
    }
  }

  const finalHtml = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hake GeoDesk — Enterprise Licensing Admin (React)</title>
  <style>
${cssContent}
  </style>
</head>
<body>
  <div id="root"></div>
  <script>
${jsContent}
  </script>
</body>
</html>`;

  const targetPath = path.join(__dirname, '../app/admin_ui.html');
  fs.writeFileSync(targetPath, finalHtml, 'utf8');
  console.log(`✓ Successfully compiled React Admin Dashboard to ${targetPath} (${(finalHtml.length / 1024).toFixed(1)} KB)`);
}

build().catch((err) => {
  console.error(err);
  process.exit(1);
});
