package org.bitskim.android;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsProvider;
import java.io.*;
import java.nio.file.Files;

/** Only debug builds expose this read-only, temporary test document provider. */
public final class FolderProviderChecks extends DocumentsProvider {
    static File fixture;
    private static final String[] DOCUMENTS={DocumentsContract.Document.COLUMN_DOCUMENT_ID,DocumentsContract.Document.COLUMN_DISPLAY_NAME,DocumentsContract.Document.COLUMN_MIME_TYPE,DocumentsContract.Document.COLUMN_SIZE,DocumentsContract.Document.COLUMN_FLAGS};
    @Override public boolean onCreate(){return true;}
    private File file(String id)throws FileNotFoundException {
        if(fixture==null)throw new FileNotFoundException("No active folder fixture");File result=id.equals("root")?fixture:new File(fixture,id);
        try{if(Files.isSymbolicLink(result.toPath())||(!result.getCanonicalFile().equals(fixture.getCanonicalFile())&&!result.getCanonicalPath().startsWith(fixture.getCanonicalPath()+File.separator)))throw new FileNotFoundException("Outside fixture");return result;}catch(IOException e){throw new FileNotFoundException(e.toString());}
    }
    private void row(MatrixCursor cursor,String id,File file){Object[] values=new Object[cursor.getColumnCount()];String[] columns=cursor.getColumnNames();for(int i=0;i<columns.length;i++){switch(columns[i]){case DocumentsContract.Document.COLUMN_DOCUMENT_ID:values[i]=id;break;case DocumentsContract.Document.COLUMN_DISPLAY_NAME:values[i]=file.getName();break;case DocumentsContract.Document.COLUMN_MIME_TYPE:values[i]=file.isDirectory()?DocumentsContract.Document.MIME_TYPE_DIR:"application/octet-stream";break;case DocumentsContract.Document.COLUMN_SIZE:values[i]=file.isFile()?file.length():null;break;case DocumentsContract.Document.COLUMN_FLAGS:values[i]=0;break;}}cursor.addRow(values);}
    @Override public Cursor queryRoots(String[] projection){MatrixCursor result=new MatrixCursor(new String[]{DocumentsContract.Root.COLUMN_ROOT_ID,DocumentsContract.Root.COLUMN_DOCUMENT_ID,DocumentsContract.Root.COLUMN_TITLE,DocumentsContract.Root.COLUMN_FLAGS});if(fixture!=null)result.addRow(new Object[]{"fixture","root","BitSkim folder check",DocumentsContract.Root.FLAG_SUPPORTS_IS_CHILD});return result;}
    @Override public Cursor queryDocument(String id,String[] projection)throws FileNotFoundException {MatrixCursor cursor=new MatrixCursor(projection==null?DOCUMENTS:projection);row(cursor,id,file(id));return cursor;}
    @Override public Cursor queryChildDocuments(String parent,String[] projection,String order)throws FileNotFoundException {MatrixCursor cursor=new MatrixCursor(projection==null?DOCUMENTS:projection);File[] children=file(parent).listFiles();if(children==null)throw new FileNotFoundException("Unreadable fixture");for(File child:children){String id=fixture.toPath().relativize(child.toPath()).toString();row(cursor,id,file(id));}return cursor;}
    @Override public boolean isChildDocument(String parent,String child) {try{return file(child).getCanonicalPath().startsWith(file(parent).getCanonicalPath()+File.separator);}catch(IOException e){return false;}}
    @Override public ParcelFileDescriptor openDocument(String id,String mode,CancellationSignal signal)throws FileNotFoundException {if(!"r".equals(mode))throw new FileNotFoundException("Read-only fixture");return ParcelFileDescriptor.open(file(id),ParcelFileDescriptor.MODE_READ_ONLY);}
}
