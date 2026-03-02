import { promises as fs } from 'fs';
import path from 'path';

const projectRoot = process.cwd();
const distDir = path.join(projectRoot, 'dist');
const dataDir = path.join(projectRoot, 'data');

async function ensureDir(dir) {
  await fs.mkdir(dir, { recursive: true });
}

async function emptyDir(dir) {
  try {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    await Promise.all(entries.map(async (entry) => {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await emptyDir(fullPath);
        await fs.rmdir(fullPath);
      } else {
        await fs.unlink(fullPath);
      }
    }));
  } catch (error) {
    if (error && error.code === 'ENOENT') return;
    throw error;
  }
}

async function copyDir(src, dest) {
  await ensureDir(dest);
  const entries = await fs.readdir(src, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      await copyDir(srcPath, destPath);
    } else {
      await fs.copyFile(srcPath, destPath);
    }
  }
}

async function main() {
  try {
    await fs.access(distDir);
  } catch {
    console.error('Vite build ciktisi bulunamadi: dist/ klasoru yok.');
    process.exit(1);
  }

  await ensureDir(dataDir);
  await emptyDir(dataDir);
  await copyDir(distDir, dataDir);

  console.log('Vite dist/ ciktisi data/ klasorune kopyalandi.');
}

main().catch((error) => {
  console.error('Kopyalama hatasi:', error);
  process.exit(1);
});
